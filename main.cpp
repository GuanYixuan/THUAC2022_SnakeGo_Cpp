#include <vector>
#include "adk.hpp"
#include "logging.cpp"
using namespace std;

const int ACT[4][2] = {{1,0},{0,1},{-1,0},{0,-1}};

AI ai;

class AI {
	public:
		AI();
		int judge(const Snake &snake, const Context &ctx);
	private:
		Logger logger;
		const Context* ctx;
		
		const Snake* snake;
};

AI::AI() {
	this->logger = Logger l();
}
int AI::judge(const Snake &snake, const Context &ctx) {
	this->snake = &snake;
	this->ctx = &ctx;

}

Operation make_your_decision( const Snake &snake_to_operate, const Context &ctx )
{
	return OP_UP;
}

int main( int argc, char **argv )
{
	
	SnakeGoAI start( argc, argv );
}