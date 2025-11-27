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
	std::istringstream ss(timeStr);
	std::chrono::sys_seconds timePoint;

	ss >> std::chrono::parse("%Y-%m-%d %H:%M:%S", timePoint);

	if (ss.fail()) {
		return 0;
	}

	return std::chrono::system_clock::to_time_t(std::chrono::time_point_cast<std::chrono::system_clock::duration>(timePoint));
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


TimeCalculator::TimeCalculator() : timestamp(0), hour(0), minute(0), second(0) {}
TimeCalculator::TimeCalculator(time_t ts) : timestamp(ts) {
	hour = (timestamp / 3600) % 24;
	minute = (timestamp / 60) % 60;
	second = timestamp % 60;
}
TimeCalculator::TimeCalculator(std::string timeStr) {
	std::vector<std::string> timeParts = StringSplit(timeStr, ':');
	hour = std::stoi(timeParts[0]);
	minute = std::stoi(timeParts[1]);
	second = std::stoi(timeParts[2]);
	timestamp = hour * 3600 + minute * 60 + second;
}
time_t TimeCalculator::GetSeconds() const { return timestamp; }
int TimeCalculator::GetHour() const { return hour; }
int TimeCalculator::GetMinute() const { return minute; }
int TimeCalculator::GetSecond() const { return second; }
bool TimeCalculator::compare(const TimeCalculator& other) const {
	return timestamp < other.timestamp;
}
bool TimeCalculator::compare(const std::string& timeStr) const {
	TimeCalculator other(timeStr);
	return timestamp < other.timestamp;
}
bool TimeCalculator::compare(time_t ts) const {
	return timestamp < ts;
}
bool TimeCalculator::operator<(const TimeCalculator& other) const {
	return compare(other);
}
bool TimeCalculator::operator<(const std::string& timeStr) const {
	return compare(timeStr);
}
bool TimeCalculator::operator>(const TimeCalculator& other) const {
	return !compare(other) && timestamp != other.timestamp;
}
bool TimeCalculator::operator>(const std::string& timeStr) const {
	return !compare(timeStr) && timestamp != TimeCalculator(timeStr).timestamp;
}
bool TimeCalculator::operator<=(const TimeCalculator& other) const {
	return !(*this > other);
}
bool TimeCalculator::operator>=(const TimeCalculator& other) const {
	return !(*this < other);
}
bool TimeCalculator::operator==(const TimeCalculator& other) const {
	return timestamp == other.timestamp;
}
bool TimeCalculator::operator==(time_t other) const {
	return timestamp == other;
}
bool TimeCalculator::operator=(const std::string& timeStr) {
	TimeCalculator other(timeStr);
	timestamp = other.timestamp;
	hour = other.hour;
	minute = other.minute;
	second = other.second;
	return true;
}
bool TimeCalculator::operator-=(int seconds) {
	timestamp -= seconds;
	hour = (timestamp / 3600) % 24;
	minute = (timestamp / 60) % 60;
	second = timestamp % 60;
	return true;
}
bool TimeCalculator::operator+=(int seconds) {
	timestamp += seconds;
	hour = (timestamp / 3600) % 24;
	minute = (timestamp / 60) % 60;
	second = timestamp % 60;
	return true;
}
TimeCalculator TimeCalculator::operator-(int seconds) const {
	TimeCalculator temp = *this;
	temp -= seconds;
	return temp;
}
TimeCalculator TimeCalculator::operator+(int seconds) const {
	TimeCalculator temp = *this;
	temp += seconds;
	return temp;
}
std::string TimeCalculator::print() const {
	std::stringstream ss;
	ss << std::setw(std::to_string(hour).length()) << std::setfill('0') << hour << ":"
		<< std::setw(2) << std::setfill('0') << minute << ":"
		<< std::setw(2) << std::setfill('0') << second;
	return ss.str();
}

CountdownTimer::CountdownTimer(time_t end) {
	endTime = std::chrono::system_clock::from_time_t(end);
}
CountdownTimer::CountdownTimer(std::string endTimeString) {
	std::istringstream ss(endTimeString);
	std::chrono::sys_seconds timePoint;

	ss >> std::chrono::parse("%Y-%m-%d %H:%M:%S", timePoint);

	endTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(timePoint) - timeZoneOffset * std::chrono::hours(1);
	std::cout << "Timestamp: " << endTime.time_since_epoch().count() << std::endl;
}
void CountdownTimer::setTimeZone(unsigned short timeZone) {
	timeZoneOffset = timeZone;
}
void CountdownTimer::begin() {
	startTime = std::chrono::system_clock::now();
}
std::string CountdownTimer::print() const {
	time_t remaining = std::chrono::duration_cast<std::chrono::seconds>(endTime - std::chrono::system_clock::now()).count();
	if (remaining < 0) remaining = 0;
	return TimeCalculator(remaining).print();
}
bool CountdownTimer::isFinished() const {
	return std::chrono::system_clock::now() - timeZoneOffset * std::chrono::hours(1) >= endTime;
}