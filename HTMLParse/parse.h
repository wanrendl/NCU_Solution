#ifndef _PARSE_H_
#define _PARSE_H_

#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

class HTML {
private:
	std::string tagName;
	std::map<std::string, std::string> attributes;
	std::map<std::string, HTML> children;
	std::string value;

public:
	HTML() = default;
	~HTML() {
		children.clear();
		attributes.clear();
		value.clear();
		tagName.clear();
	}

	// 基本编辑接口
	void setTagName(const std::string& name) {
		tagName = name;
	}

	const std::string& getTagName() const {
		return tagName;
	}

	void setAttribute(const std::string& key, const std::string& val) {
		attributes[key] = val;
	}

	bool hasAttribute(const std::string& key) const {
		return attributes.find(key) != attributes.end();
	}

	std::string getAttribute(const std::string& key) const {
		auto it = attributes.find(key);
		if (it != attributes.end()) {
			return it->second;
		}
		return "";
	}

	void removeAttribute(const std::string& key) {
		attributes.erase(key);
	}

	void clearAttributes() {
		attributes.clear();
	}

	// 文本内容编辑
	void setText(const std::string& text) {
		value = text;
	}

	const std::string& getText() const {
		return value;
	}

	// 子节点编辑（保持原有下标操作）
	HTML& operator[](const std::string& key) {
		return children[key];
	}

	const HTML& operator[](const std::string& key) const {
		auto it = children.find(key);
		if (it != children.end()) {
			return it->second;
		}
		static const HTML empty;
		return empty;
	}

	bool hasChild(const std::string& key) const {
		return children.find(key) != children.end();
	}

	void addChild(const std::string& key, const HTML& child) {
		children[key] = child;
	}

	void removeChild(const std::string& key) {
		children.erase(key);
	}

	void clearChildren() {
		children.clear();
	}

	// 赋值为文本内容（兼容原逻辑）
	HTML& operator=(const std::string& str) {
		value = str;
		return *this;
	}

	// 输出为HTML字符串
	std::string toString() const {
		// 无标签名：仅输出文本及子节点内容
		if (tagName.empty()) {
			std::ostringstream ossNoTag;
			ossNoTag << value;
			for (const auto& kv : children) {
				ossNoTag << static_cast<std::string>(kv.second);
			}
			return ossNoTag.str();
		}

		std::ostringstream oss;
		oss << "<" << tagName;
		for (const auto& kv : attributes) {
			oss << " " << kv.first << "=\"" << kv.second << "\"";
		}
		oss << ">";

		// inner text
		oss << value;

		// children
		for (const auto& kv : children) {
			oss << static_cast<std::string>(kv.second);
		}

		oss << "</" << tagName << ">";
		return oss.str();
	}

	// 转换为字符串（用于输出）
	operator std::string() const {
		return toString();
	}

	friend std::ostream& operator<<(std::ostream& os, const HTML& html) {
		os << static_cast<std::string>(html);
		return os;
	}
};

class HTMLVoidTag {
private:
	std::string tagName;
	std::map<std::string, std::string> attributes;
public:
	HTMLVoidTag(const std::string& name) : tagName(name) {}
	HTMLVoidTag() {}
	void setTagName(const std::string& name) {
		tagName = name;
	}
	void setAttribute(const std::string& key, const std::string& value) {
		attributes[key] = value;
	}
	std::string getAttribute(const std::string& key) const {
		auto it = attributes.find(key);
		if (it != attributes.end()) return it->second;
		return "";
	}
	std::string toString() const {
		std::ostringstream oss;
		oss << "<" << tagName;
		for (const auto& [key, value] : attributes) {
			oss << " " << key << "=\"" << value << "\"";
		}
		oss << ">";
		return oss.str();
	}
	friend std::ostream& operator<<(std::ostream& os, const HTMLVoidTag& tag) {
		os << tag.toString();
		return os;
	}
};

class HTMLContainerTag {
private:
	std::string tagName;
	std::map<std::string, std::string> attributes;
	std::vector<std::string> children;
public:
	HTMLContainerTag(const std::string& name) : tagName(name) {}
	HTMLContainerTag() {}
	void setTagName(const std::string& name) {
		tagName = name;
	}
	void setAttribute(const std::string& key, const std::string& value) {
		attributes[key] = value;
	}
	void addChild(const std::string& child) {
		children.push_back(child);
	}
	std::string toString() const {
		std::ostringstream oss;
		oss << "<" << tagName;
		for (const auto& [key, value] : attributes) {
			oss << " " << key << "=\"" << value << "\"";
		}
		oss << ">";
		for (const auto& child : children) {
			oss << child;
		}
		oss << "</" << tagName << ">";
		return oss.str();
	}
	friend std::ostream& operator<<(std::ostream& os, const HTMLContainerTag& tag) {
		os << tag.toString();
		return os;
	}
};

class HTMLParser {
private:
	std::map<std::string, HTMLVoidTag> voidTags;
	std::map<std::string, HTMLContainerTag> containerTags;
	std::vector<std::string> tagNames;
	std::map<std::string, bool> isVoidTag;
public:
	HTMLParser(std::string html) {
		bool doContinue = true;
		std::string::iterator s;
		for (std::string::iterator htmlit = html.begin(); htmlit != html.end(); ++htmlit) {
			
			if (*htmlit == '<') {
				s = htmlit;
				doContinue = false;
			}
			if (doContinue) continue;

			bool flag1 = false, flag2 = false;
			if (*htmlit == '\"') {
				flag1 = true;
				if (flag1) flag1 = false;
			}
			if (*htmlit == '\'') {
				flag2 = true;
				if (flag2) flag2 = false;
			}
					
			if (flag1 || flag2) continue;

			if (*htmlit == '/' && *(htmlit + 1) == '>') {
				std::string tagContent(s + 1, htmlit);
				std::cout << tagContent << std::endl;
				HTMLVoidTag voidTag;
				std::string content = "", tagName, key;
				int flag = 0;
				for (auto& ch : tagContent) {
					content += ch;
					if (ch == ' ' && flag == 0) {
						tagName = content;
						flag += 1;
						std::cout << tagName << std::endl;
						content.clear();
					}
					else if (ch == '=' && flag == 1) {
						content.pop_back();
						key = content;
						flag += 1;
						std::cout << key << std::endl;
						content.clear();
					}
					else if (ch == ' ' && flag == 2) {
						content.pop_back();
						voidTag.setAttribute(key, content);
						flag -= 1;
						std::cout << content << std::endl;
						content.clear();
					}
				}
				voidTag.setAttribute(key, content);
				std::cout << content << std::endl;
				tagNames.push_back(tagName);
				isVoidTag[tagName] = true;
				voidTags[tagName] = voidTag;
			}
		}
	}
	
	HTMLVoidTag getVoidTag(std::string tagName) {
		return voidTags[tagName];
	}
	HTMLContainerTag getContainerTag(std::string tagName) {
		return containerTags[tagName];
	}
};

#endif // !_PARSE_H_