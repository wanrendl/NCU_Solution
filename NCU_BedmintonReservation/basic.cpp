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

CurrentTime::CurrentTime() {
	now = std::chrono::system_clock::now();
	currentTime = std::chrono::system_clock::to_time_t(now);
}
time_t CurrentTime::GetSeconds() {
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
int CurrentTime::GetSecond() {
	now = std::chrono::system_clock::now();
	currentTime = std::chrono::system_clock::to_time_t(now);
	std::tm* local_tm = std::localtime(&currentTime);
	return local_tm->tm_sec;
}
int CurrentTime::GetMillisecond() {
	now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() % 1000;
	return static_cast<int>(milliseconds);
}

time_t StringToTimeStamp(const std::string timeStr) {
	std::tm tm = {};
	std::istringstream ss(timeStr);
	ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
	if (ss.fail()) return 0;
	return std::chrono::system_clock::to_time_t(std::chrono::system_clock::from_time_t(std::mktime(&tm)));
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

DateCalculator DateCalculator::operator-(const DateCalculator& other) const {
	// 计算两个日期之间的天数差
	DateCalculator start = *this;
	DateCalculator end = other;
	if (start > end) std::swap(start, end);
	int days = 0;
	while (start < end) {
		start.addDays(1);
		days++;
	}
	return DateCalculator(std::to_string(days));
}

DateCalculator DateCalculator::operator+(const DateCalculator& other) const {
	// 将两个日期相加，返回一个新的日期
	DateCalculator result = *this;
	result.addDays(other.day);
	result.addMonths(other.month);
	result.addYears(other.year);
	return result;
}

CountdownTimer::CountdownTimer() : startTime(std::chrono::seconds(0)), endTime(std::chrono::seconds(0)) {}
CountdownTimer::CountdownTimer(time_t endstamp) : startTime(std::chrono::seconds(0)) {
	endTime = std::chrono::seconds(endstamp);
}
CountdownTimer::CountdownTimer(std::string timeString) : startTime(std::chrono::seconds(0)) {
	std::tm tm = {};
	std::istringstream ss(timeString);
	ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
	if (ss.fail()) {
		throw std::runtime_error("时间字符串解析失败: " + timeString);
	}
	auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
	endTime = std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch());
}
CountdownTimer::CountdownTimer(std::string date, std::string clock) : startTime(std::chrono::seconds(0)) {
	std::tm tm = {};
	std::istringstream ss(date + " " + clock);
	ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
	if (ss.fail()) {
		throw std::runtime_error("时间字符串解析失败: " + date + " " + clock);
	}
	auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
	endTime = std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch());
	std::cout << "Endtime: " << endTime.count() << std::endl;
}
void CountdownTimer::begin() {
	startTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
}
bool CountdownTimer::isFinished() const {
	return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()) >= endTime;
}
int CountdownTimer::getRemainingSeconds() const {
	auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
	if (now >= endTime) return 0;
	return std::chrono::duration_cast<std::chrono::seconds>(endTime - now).count();
}
bool CountdownTimer::compare(std::string time_str) {
	if (StringToTimeStamp(time_str) == std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())) return true;
	return false;
}
bool CountdownTimer::compare(std::string time_date, std::string time_clock) {
	std::chrono::seconds targetTime = this->StringToTimeStamp(time_date + " " + time_clock);
	std::chrono::seconds currentTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
	if (targetTime == currentTime) return true;
	return false;
}
std::string CountdownTimer::getRemainingTimeString() const {
	int totalSeconds = getRemainingSeconds();
	int hours = totalSeconds / 3600;
	int minutes = (totalSeconds % 3600) / 60;
	int seconds = totalSeconds % 60;
	std::ostringstream oss;
	oss << std::setw(std::to_string(hours).length()) << std::setfill('0') << hours << ":"
		<< std::setw(2) << std::setfill('0') << minutes << ":"
		<< std::setw(2) << std::setfill('0') << seconds;
	return oss.str();
}
std::chrono::seconds CountdownTimer::StringToTimeStamp(const std::string timeStr) {
	std::tm tm = {};
	std::istringstream ss(timeStr);
	ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
	if (ss.fail()) return std::chrono::seconds(0);
	auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
	return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch());
}