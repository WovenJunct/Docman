#pragma once
#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <sstream>
#include<fstream>
#include<iostream>
const std::string API_ENDPOINT{"http://docman.zhuof.wang"};

inline std::string encodeUriComponent(const std::string& s) {
    std::string encoded;
    char c;
    for (size_t i = 0; i < s.length(); i++) {
        c = s[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else if (c == ' ') {
            encoded += '+';
        } else {
            encoded += '%';
            // convert to hex
            std::stringstream ss;
            ss << std::hex << (int) c;
            encoded += ss.str();
        }
    }
    return encoded;
}

//将文件内容读入一个字符串
inline std::string readFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Can't open the file" << path << std::endl;
        std::exit(1);
    }
    std::ostringstream buf;
    buf << file.rdbuf();
    return buf.str();
}
#endif 
