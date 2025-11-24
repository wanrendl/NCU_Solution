#include "basic.h"

std::string ReadStrngFromFile(std::string fileName) {
	std::ifstream file(fileName, std::ios::binary);
	std::stringstream sStream;

	sStream << file.rdbuf();
	file.close();
	return sStream.str();
}

std::string ParseStringPos(std::string str, size_t pos1, size_t pos2) {
	return str.substr(pos1, pos2 - pos1 + 1);
}

// 从 tm 结构体中提取各部分
//int year = local_tm->tm_year + 1900; // tm_year 从1900年算起
//int month = local_tm->tm_mon + 1;    // tm_mon 从0开始
//int day = local_tm->tm_mday;
//int hour = local_tm->tm_hour;
//int minute = local_tm->tm_min;
//int second = local_tm->tm_sec;

void DelayPrint(std::string s, int ms = 50) {
	for (auto& it : s) {
		std::cout << it;
		Sleep(ms);
	}
}

void ColorfulPrint(std::string s, WORD wAttributes) {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	SetConsoleTextAttribute(hConsole, wAttributes);
	std::cout << s;
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // 重置为默认颜色
}

time_t CurrentTime::GetTimeStamp() {
	now = std::chrono::system_clock::now();
	currentTime = std::chrono::system_clock::to_time_t(now);
	return currentTime;
}
std::string CurrentTime::GetFormattedTime() {
	now = std::chrono::system_clock::now();
	currentTime = std::chrono::system_clock::to_time_t(now);
	std::stringstream t;
	t << std::put_time(std::localtime(&currentTime), "%Y-%m-%d %H:%M:%S");
	return t.str();
}
std::string CurrentTime::GetFormattedTimeDate() {
	now = std::chrono::system_clock::now();
	currentTime = std::chrono::system_clock::to_time_t(now);
	std::stringstream t;
	t << std::put_time(std::localtime(&currentTime), "%Y-%m-%d");
	return t.str();
}
std::string CurrentTime::GetFormattedTimeAfter(int days) {
	now = std::chrono::system_clock::now();
	currentTime = std::chrono::system_clock::to_time_t(now) + static_cast<time_t>(days) * 24 * 60 * 60;
	std::stringstream t;
	t << std::put_time(std::localtime(&currentTime), "%Y-%m-%d");
	return t.str();
}
int CurrentTime::GetHour() {
	now = std::chrono::system_clock::now();
	currentTime = std::chrono::system_clock::to_time_t(now);
	std::tm* local_tm = std::localtime(&currentTime);
	return local_tm->tm_hour;
}
int CurrentTime::GetMinute() {
	now = std::chrono::system_clock::now();
	currentTime = std::chrono::system_clock::to_time_t(now);
	std::tm* local_tm = std::localtime(&currentTime);
	return local_tm->tm_min;
}

time_t StringToTimeStamp(std::string str) {
	struct tm tm_ {};
	int year, month, day;
	sscanf_s(str.c_str(), "%d-%d-%d", &year, &month, &day);
	tm_.tm_year = year - 1900;
	tm_.tm_mon = month - 1;
	tm_.tm_mday = day - 2;
	tm_.tm_hour = 12;
	tm_.tm_min = 0;
	tm_.tm_sec = 0;
	tm_.tm_isdst = 0;

	time_t timeStamp = mktime(&tm_);
	return timeStamp;
}

std::vector<std::string> StringSplit(std::string str, char delimiter) {
	std::vector<std::string> result;
	std::stringstream ss(str);
	std::string item;
	while (std::getline(ss, item, delimiter)) result.push_back(item);
	return result;
}

DateCalculator::DateCalculator(std::string date) {
	std::vector<std::string> dateParts = StringSplit(date, '-');
	year = std::stoi(dateParts[0]);
	month = std::stoi(dateParts[1]);
	day = std::stoi(dateParts[2]);
}

DateCalculator::DateCalculator() {
	year = 1970, month = 1, day = 1;
}

bool DateCalculator::addDays(int days) {
	day += days;
	static const int daysInMonth[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
	while (true) {
		int daysThisMonth = daysInMonth[month - 1];
		if (month == 2 && isLeapYear(year)) daysThisMonth++;
		if (day <= daysThisMonth) break;
		day -= daysThisMonth;
		month++;
		if (month > 12) {
			month = 1;
			year++;
		}
	}
	return true;
}
bool DateCalculator::addMonths(int months) {
	month += months;
	while (month > 12) {
		month -= 12;
		year++;
	}
	return true;
}
bool DateCalculator::addYears(int years) {
	year += years;
	return true;
}
std::string DateCalculator::Print() const {
	std::stringstream ss;
	ss << year << "-";
	if (month < 10) ss << "0";
	ss << month << "-";
	if (day < 10) ss << "0";
	ss << day;
	return ss.str();
}

bool DateCalculator::isLeapYear(int y) const {
	return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

bool DateCalculator::isValidDate() const {
	if (month < 1 || month > 12) return false;
	std::vector<int> daysInMonth = { 31,28,31,30,31,30,31,31,30,31,30,31 };
	int daysThisMonth = daysInMonth[month - 1];
	if (month == 2 && isLeapYear(year)) daysThisMonth++;
	return day >= 1 && day <= daysThisMonth;
}

bool DateCalculator::setDate(std::string date) {
	std::vector<std::string> dateParts = StringSplit(date, '-');
	if (dateParts.size() != 3) return false;
	year = std::stoi(dateParts[0]);
	month = std::stoi(dateParts[1]);
	day = std::stoi(dateParts[2]);
	return isValidDate();
}

bool DateCalculator::operator<(const DateCalculator& other) const {
	if (year < other.year) return true;
	if (year > other.year) return false;
	if (month < other.month) return true;
	if (month > other.month) return false;
	if (day < other.day) return true;
	return false;
}
bool DateCalculator::operator<(const std::string& date) const {
	DateCalculator other(date);
	return *this < other;
}
bool DateCalculator::operator>(const DateCalculator& other) const {
	if (year > other.year) return true;
	if (year < other.year) return false;
	if (month > other.month) return true;
	if (month < other.month) return false;
	if (day > other.day) return true;
	return false;
}
bool DateCalculator::operator>(const std::string& date) const {
	DateCalculator other(date);
	return *this > other;
}
bool DateCalculator::operator<=(const DateCalculator& other) const {
	return !(*this > other);
}
bool DateCalculator::operator>=(const DateCalculator& other) const {
	return !(*this < other);
}
bool DateCalculator::operator==(const DateCalculator& other) const {
	return (year == other.year && month == other.month && day == other.day);
}
bool DateCalculator::operator=(const std::string& date) {
	return setDate(date);
}
bool DateCalculator::operator-=(int days) {
	return addDays(-days);
}
DateCalculator DateCalculator::operator-(int days) const {
	DateCalculator temp = *this;
	temp.addDays(-days);
	return temp;
}
bool DateCalculator::operator+=(int days) {
	return addDays(days);
}
DateCalculator DateCalculator::operator+(int days) const {
	DateCalculator temp = *this;
	temp.addDays(days);
	return temp;
}