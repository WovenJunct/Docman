#pragma once
#ifndef CITATION_H
#define CITATION_H

#include <string>
#include <nlohmann/json.hpp>
#include<cpp-httplib/httplib.h>

class Citation {
public:
	std::string id;
	explicit Citation(const std::string& id);
	virtual ~Citation() = default;
	//格式化好的字符串
	virtual std::string format() const = 0;
	//网络补全信息
	virtual void fetchInfo(httplib::Client &client) = 0;
	//根据json的type创建对应子类
	static Citation* fromJson(const nlohmann::json& obj);
protected:
	//检查obj中某字段存在且为string
	static void requireString(const nlohmann::json& obj, const std::string& key, const std::string& id);
	//检查obj中某字段存在且为int
	static void requireInt(const nlohmann::json& obj, const std::string& key, const std::string& id);
};

//子类一：书籍
class BookCitation :public Citation {
	std::string isbn;
	std::string author;
	std::string title;
	std::string publisher;
	int year{};
public:
	explicit BookCitation(const nlohmann::json& obj);
	std::string format() const override;
	void fetchInfo(httplib::Client& client) override;
};

//子类二：网页
class WebpageCitation :public Citation {
	std::string url;
	std::string title;
public:
	explicit WebpageCitation(const nlohmann::json& obj);
	std::string format() const override;
	void fetchInfo(httplib::Client& client) override;
};

//子类三：文章
class ArticleCitation :public Citation {
	std::string author;
	std::string title;
	std::string journal;
	int year{};
	int volume{};
	int issue{};
public:
	explicit ArticleCitation(const nlohmann::json& obj);
	std::string format() const override;
	void fetchInfo(httplib::Client& client) override;
};
#endif