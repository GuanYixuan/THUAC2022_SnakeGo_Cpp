#include <map>
#include <vector>
#include "adk.hpp"
#include "assess.cpp"
#include "logging.cpp"
using namespace std;

const int ACT[4][2] = {{1,0},{0,1},{-1,0},{0,-1}};

Logger logger = Logger();
AI ai = AI(logger);

class Assess;
class AI {
	public:
		AI(Logger &logger);
		int judge(const Snake &snake, const Context &ctx);
	private:
		Logger &logger;
		Assess* assess;
		const Context* ctx;
		const Snake* snake;

		map<int,Item> wanted_item;
};

AI::AI(Logger &logger) : 
logger(logger)
{
	
}
int AI::judge(const Snake &snake, const Context &ctx) {
	this->snake = &snake;
	this->ctx = &ctx;
	this->assess = Assess(*this,ctx,this->logger,snake.id);
}

Operation make_your_decision( const Snake &snake, const Context &ctx, const OpHistory& op_history)
{
	return ai.judge(snake,ctx);
}
void game_over( int gameover_type, int winner, int p0_score, int p1_score )
{
	fprintf( stderr, "%d %d %d %d", gameover_type, winner, p0_score, p1_score );
}
int main( int argc, char **argv )
{
	SnakeGoAI start( argc, argv );
}