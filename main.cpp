#include <fstream>
#include <iostream>
#include <vector>
#include<map>
#include<set>
#include<sstream>
#include<algorithm>
#include<cstddef>

#include "utils.hpp"
#include "citation.h"

//命令行参数
struct Args {
    std::string citationPath;
    std::string outputPath;
    std::string inputPath;
};
Args parseArgs(int argc, char** argv) {
    Args args;
    bool hasCitation = false;
    bool hasOutput = false;
    bool hasInput = false;
    for (int i = 1;i < argc;) {
        std::string token(argv[i]);
        if (token == "-c") {
            if (hasCitation) {
                std::cerr << "Error:-c specified more than once" << std::endl;
                std::exit(1);
            }
            if (i + 1 >= argc) {
                std::cerr << "Error:-c requires a filename" << std::endl;
                std::exit(1);
            }
            args.citationPath = argv[i + 1];
            hasCitation = true;
            i += 2;
        }
        else if (token == "-o") {
            if (hasOutput) {
                std::cerr << "Error:-o specified more than once" << std::endl;
                std::exit(1);
            }
            if (i + 1 >= argc) {
                std::cerr << "Error:-o requires an argument" << std::endl;
                std::exit(1);
            }
            args.outputPath = argv[i + 1];
            hasOutput = true;
            i += 2;
        }
        else if (token[0] == '-' && token.size() > 1) {
            std::cerr << "Error.unknowen option:" << token << std::endl;
            std::exit(1);
        }
        else {
            if (hasInput) {
                std::cerr << "Error:multiple input files specified" << std::endl;
                std::exit(1);
            }
            args.inputPath = token;
            hasInput = true;
            i += 1;
        }
    }
    if (!hasCitation) {
        std::cerr << "Error: -c citation_path is required" << std::endl;
        std::exit(1);
    }
    if (!hasInput) {
        std::cerr << "Error: input file is required" << std::endl;
        std::exit(1);
    }
    return args;
}

//加载文献合集
std::vector<Citation*> loadCitations(const std::string& filename) {
    // FIXME: load citations from file
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error:can't open the file  " << filename << std::endl;
        std::exit(1);
    }

    nlohmann::json data = nlohmann::json::parse(file);
    if (!data.is_object()) {
        std::cerr << "Error: citation file root must be a JSON object" << std::endl;
        std::exit(1);
    }
    if (!data.contains("citations") || !data["citations"].is_array()) {
        std::cerr << "Error: citation file must have a 'citations' array" << std::endl;
        std::exit(1);
    }

    std::vector<Citation*> citations;
    std::set<std::string>seenIds; //检测重复Id

    for (const auto& obj : data["citations"]) {
        Citation* c = Citation::fromJson(obj);
        if (seenIds.count(c->id)) {
            std::cerr << "Error: duplicate citation id: " << c->id << std::endl;
            delete c;
            for (auto* ptr : citations)delete ptr;
            std::exit(1);
        }
        seenIds.insert(c->id);
        citations.push_back(c);
    }
    return citations;
}

//读取输入正文
std::string readInput(const std::string path) {
    if (path == "-") {
        std::ostringstream buf;
        buf << std::cin.rdbuf();
        return buf.str();
    }
    return readFromFile(path);
}


//提取出id，按首次出现顺序去重
std::vector<std::string> extractIds(const std::string& text) {
    std::vector<std::string> ordered;
    std::set<std::string> seen;

    bool inBracket = false;
    std::string current;

    for(size_t i = 0;i < text.size();++i) {
        char ch = text[i];
        if (ch == '[') {
            if (inBracket) {
                std::cerr << "Error: unexpected '[' inside a citation at position "
                    << i << std::endl;
                std::exit(1);
            }
            inBracket = true;
            current.clear();
        }
        else if (ch == ']') {
            if (!inBracket) {
                std::cerr << "Error: unexpected ']' without matching '[' at position "
                    << i << std::endl;
                std::exit(1);
            }
            inBracket = false;
            if (!seen.count(current)) {
                seen.insert(current);
                ordered.push_back(current);
            }
        }
        else if (inBracket) {
            current += ch;
        }
    }
    if (inBracket) {
        std::cerr << "Error: unclosed '[' in input text" << std::endl;
        std::exit(1);
    }
    return ordered;
}

//主程序
//
int main(int argc, char** argv) {
    // "docman", "-c", "citations.json", "input.txt"
    //解析参数   可改变参数顺序
    Args args = parseArgs(argc, argv);
	//加载文献合集
    auto citations = loadCitations(args.citationPath);
    // FIXME: read all input to the string, and process citations in the input text
    // auto input = readFromFile(argv[3]);
    // ...
    // 
    // 
    //读取正文
    auto input = readInput(args.inputPath);
	//提取输入文本中的id
    std::vector<std::string> useIds = extractIds(input);
    //转换为map方便查找
    std::map<std::string, Citation*> citationMap;
    for (auto* c : citations) {
        citationMap[c->id] = c;
    }

    //验证每个id均存在
    for (const auto& id : useIds) {
        if (!citationMap.count(id)) {
            std::cerr << "Error: citation id [" << id
                << "] not found in citation file" << std::endl;
            for (auto* c : citations) delete c;
            std::exit(1);
        }
    }
    //收集需要输出的文献，按照id字符串顺序排序
    std::vector<Citation*> printedCitations;
    {
        std::vector<std::string> sortedIds = useIds;
        std::sort(sortedIds.begin(), sortedIds.end());
        for (const auto& id : sortedIds) {
            printedCitations.push_back(citationMap[id]);
        }
    }

    // 网络查询
    httplib::Client client{ API_ENDPOINT };
    for (auto* c : printedCitations) {
        c->fetchInfo(client);
    }

    // 组装完整输出到缓冲区
    std::ostringstream buf;
    buf << input;  
    buf << "\n\nReferences:\n";
    for (auto c : printedCitations) {
        buf << c->format() << "\n";  
    }

    // 写出
    if (args.outputPath.empty()) {
        std::ostream& output = std::cout;
        output << buf.str();
    }
    else {
        std::ofstream outFile(args.outputPath);
        if (!outFile.is_open()) {
            std::cerr << "Error: cannot create output file: "
                << args.outputPath << std::endl;
            for (auto* c : citations) delete c;
            std::exit(1);
        }
        outFile << buf.str();
    }


    //std::ostream& output = std::cout;

    // output << input;  // print the paragraph first
    // output << "\n\nReferences:\n";
    
    //for (auto c : printedCitations) {
    //    // FIXME: print citation
    //}

    for (auto c : citations) {
        delete c;
    }
}
