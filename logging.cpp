#include <cstdio>
#include <cstdarg>
#include "adk.hpp"
using namespace std;

const bool LOG_SWITCH = true;
const int LOG_LEVEL = 3;

class Logger {
	public:
		Logger();
		void config(int turn, int snkid);
		void log(int level, const char* format, ...);
		void flush();//每回合结束要flush
	private:
		int turn;
		int snkid;
		char buffer[105];
		std::FILE* file;
};

Logger::Logger() {
	if(LOG_SWITCH) fopen("log.log","a");
}
void Logger::config(int turn, int snkid) {
	this->turn = turn;
	this->snkid = snkid;
}
void Logger::log(int level, const char* format, ...) {
	if(LOG_SWITCH && level > LOG_LEVEL) {
		va_list args;
		va_start(args,format);
		vsprintf(this->buffer,format,args);
		va_end(args);
		fprintf(this->file,"turn:%3d snk:%2d %s\n",this->turn,this->snkid,this->buffer);
	}
}
void Logger::flush() {
	if(LOG_SWITCH) fflush(this->file);
}