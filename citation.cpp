#include "citation.h"
#include "utils.hpp"

#include <algorithm>
#include<iostream>
#include<sstream>

//Citation基类
Citation::Citation(const std::string& rawId) :id(rawId) {
	//id非空且只包含字母和数字
	if (id.empty()) {
		std::cerr << "Erroe:citation id is empty" << std::endl;
		std::exit(1);
	}
	bool valid = std::all_of(id.begin(), id.end(), [](unsigned char c) {
		return std::isalnum(c);
		});

	if (!valid) {
		std::cerr << "Error:invalid citation id: " << id << std::endl;
		std::exit(1);
	}
}
//检查字段存在且为字符串类型
void Citation::requireString(const nlohmann::json& obj, const std::string& key, const std::string& id) {
	if (!obj.contains(key) || !obj[key].is_string()) {
		std::cerr << "Error: missing or invalid field '" << key << "' in citation " << id << std::endl;
		std::exit(1);
	}
}
//检查字段存在且为整数类型
void Citation::requireInt(const nlohmann::json& obj, const std::string& key, const std::string& id) {
	if (!obj.contains(key) || !obj[key].is_number_integer()) {
		std::cerr << "Error: missing or invalid field '" << key << "' in citation " << id << std::endl;
		std::exit(1);
	}
}
//根据json的type创建对应子类
Citation* Citation::fromJson(const nlohmann::json& obj) {
	if (!obj.is_object()) {
		std::cerr << "Error: citation entry is not a JSON object" << std::endl;
		std::exit(1);
	}
	// id 和 type 是所有文献都必须有的字段
	requireString(obj, "id", ""); 
	requireString(obj, "type","");

	const std::string type = obj["type"].get<std::string>();

	if (type == "book")    return new BookCitation(obj);
	if (type == "webpage") return new WebpageCitation(obj);
	if (type == "article") return new ArticleCitation(obj);

	std::cerr << "Error: unknown citation type: " << type << std::endl;
	std::exit(1);
}

//BookCitation类
BookCitation::BookCitation(const nlohmann::json& obj) : Citation(obj["id"].get<std::string>()) {
	requireString(obj, "isbn", id);   // 传入id
	isbn = obj["isbn"].get<std::string>();
}

void BookCitation::fetchInfo(httplib::Client& client) {
	auto res = client.Get("/isbn/" + encodeUriComponent(isbn));
	if (!res) {
		std::cerr << "Error:HTTP request failed for isbn:" << isbn << std::endl;
		std::exit(1);
	}
	if (res->status != httplib::OK_200) {
		std::cerr << "Error:server returned status " << res->status << "for isbn:" << isbn << std::endl;
		std::exit(1);
	}

	//解析返回的json
	nlohmann::json data;
	try {
		data = nlohmann::json::parse(res->body);
	}
	catch (...) {
		std::cerr << "Error:failed to parse response for isbn"<<isbn <<std::endl;
		std::exit(1);
	}

	//读取字段
	requireString(data, "author",id);
	requireString(data, "title",id);
	requireString(data, "publisher",id);
	// 兼容 year 是字符串或数字
	if (!data.contains("year")) {
		std::cerr << "Error: missing field 'year' in citation " << id << std::endl;
		std::exit(1);
	}

	if (data["year"].is_number_integer()) {
		year = data["year"].get<int>();
	}
	else if (data["year"].is_string()) {
		year = std::stoi(data["year"].get<std::string>());
	}
	else {
		std::cerr << "Error: invalid field 'year' type in citation " << id << std::endl;
		std::exit(1);
	}
	author = data["author"].get<std::string>();
	title = data["title"].get<std::string>();
	publisher = data["publisher"].get<std::string>();
	//year = data["year"].get<std::string>();
}

std::string BookCitation::format() const {
	return "[" + id + "] book:" + author + ", " + title + ", "
		+ publisher + ", " + std::to_string(year);
}

//网页
WebpageCitation::WebpageCitation(const nlohmann::json& obj) :
	Citation(obj["id"].get<std::string>()) {
	requireString(obj, "url",id);
	url = obj["url"].get<std::string>();
}

void WebpageCitation::fetchInfo(httplib::Client& client) {
	auto res = client.Get("/title/" + encodeUriComponent(url));
	if (!res) {
		std::cerr << "Error:HTTP request failed for url:" << url << std::endl;
		std::exit(1);
	}
	if (res->status != httplib::OK_200) {
		std::cerr << "Error:server returned status " << res->status << "for url:" << url << std::endl;
		std::exit(1);
	}
	nlohmann::json data;
	try {
		data = nlohmann::json::parse(res->body);
	}
	catch (...) {
		std::cerr << "Error:failed to parse response for url:" << url << std::endl;
		std::exit(1);
	}
	requireString(data, "title",id);
	title = data["title"].get<std::string>();
}

std::string WebpageCitation::format() const {
	return "[" + id + "] webpage:" + title + ". Available at" + url;
}

//直接读取文章字段
ArticleCitation::ArticleCitation(const nlohmann::json& obj) :
	Citation(obj["id"].get<std::string>()) {
	requireString(obj, "author",id);
	requireString(obj, "title",id);
	requireString(obj, "journal",id);
	requireInt(obj, "year",id);
	requireInt(obj, "volume",id);
	requireInt(obj, "issue",id);

	author = obj["author"].get<std::string>();
	title = obj["title"].get<std::string>();
	journal = obj["journal"].get<std::string>();
	year = obj["year"].get<int>();
	volume = obj["volume"].get<int>();
	issue = obj["issue"].get<int>();
}

void ArticleCitation::fetchInfo(httplib::Client& /*client*/) {}

std::string ArticleCitation::format() const {
	return "[" + id + "] article:" + author + ", " + title + ", " +
		journal + ", " + std::to_string(year) + ", " +
		std::to_string(volume) + ", " + std::to_string(issue);
}