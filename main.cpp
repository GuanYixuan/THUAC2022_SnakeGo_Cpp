#include <cassert>
#include <cstdarg>
#include <iostream>
#include <map>
#include "adk.hpp"
using namespace std;

//Logger类
const bool LOG_SWITCH = false;
const int LOG_LEVEL = 0;
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
	if(LOG_SWITCH) file = fopen("log.log","a");
}
void Logger::config(int turn, int snkid) {
	this->turn = turn;
	this->snkid = snkid;
}
void Logger::log(int level, const char* format, ...) {
	if(LOG_SWITCH && level >= LOG_LEVEL) {
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

//Ai类
const int ITEM_ALLOC_MAX = 512;
struct tgt_alloc_t {
	int snkid;
	double cost;
	Item item;
};
bool tgt_alloc_cmp(const tgt_alloc_t &a, const tgt_alloc_t &b);
class Assess;
class AI {
	public:
		AI(Logger &logger);
		Logger &logger;
		int judge(const Snake &snake, const Context &ctx);

		//index是item.id，val是该物品属于的蛇的id
		int item_alloc[ITEM_ALLOC_MAX];
		
		//key是snkid，val是此蛇当前的目标
		//如果没有目标，val应为常量NULL_ITEM
		map<int,Item> wanted_item;

		void total_init();
		void turn_control();

		//[总控函数]为所有蛇分配目标
		void distribute_tgt();

		bool try_split();
		bool try_shoot();
		int try_eat();
		int try_solid();

	private:
		Assess* assess;
		int turn;
		const Context* ctx;
		const Snake* snake;

		int last_turn = -1;

		const static int ALLOC_FTR_LIMIT = 20;
		const static int ALLOC_COMPETE_LIMIT = 7;
		constexpr static double ALLOC_SLOW_COST_PENA = 1;
		constexpr static double ALLOC_APPLE_SIZ_GAIN = 1.5;
		constexpr static double ALLOC_LASER_AS_APPLE = 1;
		constexpr static double ALLOC_HAS_LASER_PENA = 7;
		constexpr static double ALLOC_MAX_COST_BOUND = 20;

		const static int SOL_COUNT_MIN = 4;
		constexpr static double SOL_SAFE_THRESH = -12;
		constexpr static double SOL_HI_SNAKE_BONUS = 2;
		constexpr static double SOL_LONG_SNK_PARAM[2] = {12,0.5};
		constexpr static double SOL_SCORE_THRESH = 3;

		constexpr static double SPLIT_THRESH = -12;

		static double val_func0(const Context& ctx);
};


//Assess类
#define MAP_LENGTH 16
typedef pair<int,int> pii;
typedef int map_int_t[MAP_LENGTH][MAP_LENGTH];
const int ACT_LEN = 4;
const int ACT[4][2] = {{1,0},{0,1},{-1,0},{0,-1}};
const int ACT_MAXV = 6;

struct act_score_t {
	int actid;
	double val;

	bool operator==(const act_score_t &b) const {return actid == b.actid && val == b.val;}
	bool operator!=(const act_score_t &b) const {return !((*this) == b);}
};
const act_score_t ACT_SCORE_NULL = act_score_t({-100,-1});
bool act_score_cmp(const act_score_t &a, const act_score_t &b);
struct spd_map_t {
	int dist;
	int snkid;

	bool operator==(const spd_map_t &b) const {return dist == b.dist && snkid == b.snkid;}
	bool operator!=(const spd_map_t &b) const {return !((*this) == b);}
};
const spd_map_t SPD_MAP_NULL = spd_map_t({-1,-1});
struct find_path_q_t {
	int x,y,step;
};
struct scan_act_q_t {
	int x,y,step;
	double val;
};
class Assess {
	public:
		Assess(AI &ai, const Context &ctx, Logger &logger, int snkid);

		spd_map_t friend_spd[MAP_LENGTH][MAP_LENGTH];
		spd_map_t enemy_spd[MAP_LENGTH][MAP_LENGTH];
		spd_map_t tot_spd[MAP_LENGTH][MAP_LENGTH];
		double mixed_score[ACT_LEN];
		double act_score[ACT_LEN],safe_score[ACT_LEN];
		double attack_score[ACT_LEN],polite_score[ACT_LEN];
		map<int, map_int_t*> dist_map;//key是snkid
		map<int, map_int_t*> path_map;

		//做一些只在每回合开始时要做的事
		//应该在do_snake_assess后调用
		void do_turn_assess();
		void do_snake_assess();
		//把attack/polite/safe分都算出来，并汇总到mixed_score里面
		//不会调用refresh_all_bfs
		void calc_mixed_score();
		//对所有蛇做一次bfs，并清除已死蛇的数据（会考虑即将吃到的食物）
		//这会更新dist_map,path_map
		void refresh_all_bfs();
		
		//计算“速度势力图”，即哪方能先到达指定格
		//【默认蛇身/头所在格到达时间为0】
		void calc_spd_map();
		

		inline int rev_step(int st);
		//不设任何引导，根据mixed_score的最优值走一格
		int random_step();
		//设置一个贪心走向tgt的引导，根据它与mixed_score之和的最优值走一格
		int greedy_step(const Coord &tgt);
		//根据find_path_bfs的结果设置一个走向tgt的引导，根据它与mixed_score之和的最优值走一格
		int find_path(const Coord &tgt);
		//处理紧急情况
		int emergency_handle();
		//释放snkid的目标物品
		void release_target(int snkid = -1);

		//返回一个vector<Item>，其中是已被此蛇稳吃的所有Item，可手动限定type
		//【可能有效率问题】
		vector<Item> get_captured_items(int snkid = -1, int item_tp = -1);
		//检查物品是否被哪方的蛇占住了（可以用身子直接吃掉），没有则返回-1
		int check_item_captured_team(const Item &item);
		//检查物品是否已经被snkid的蛇占住了（可以用身子直接吃掉）
		bool check_item_captured(const Item &item, int snkid = -1);
		//返回pos格是蛇上的第几格，需保证pos格有蛇
		int get_pos_on_snake(const Coord &pos);
		//计算当前位于pos的蛇头有几个方向可走，可添加一个额外堵塞位置extra_block及一个额外可行位置extra_go
		//【这里默认extra_block是你的蛇头而extra_go是蛇尾】
		int calc_snk_air(const Coord &pos,const Coord &extra_block = Coord({-1,-1}),const Coord &extra_go = Coord({-1,-1}));
		//计算snkid立刻主动进行固化能利用的最大身体长度，返回(最大长度,对应ACT下标)
        //如果无法进行固化，则返回(-1,-1)
		pii get_enclosing_leng(int snkid = -1);

		//检查编号为first的蛇的下一次行动是否比编号为second的蛇先
		//【这一判断基于目前正在行动的蛇的id（即self.snkid）作出】
		bool check_first(int first, int second);
		//判断id=snkid的蛇在接下来的第step步移动后走到(tx,ty)这一格是否不会被撞死
		bool check_nstep_norm(int tx, int ty, int step = 1, int snkid = -1, int self_bloc_bank_val = -1);

		bool can_split(int snkid = -1);
		bool has_laser(int snkid = -1);
		bool can_shoot(int snkid = -1);
		//如果snkid立即发射激光，会打掉多少(自己，对方)的墙？
		//注意：该函数不检查snkid能否发射激光，并对长度为1的蛇返回(-1,-1)
		pii ray_trace(int snkid = -1);
		//从pos向dire向量方向发射激光，会打掉多少(自己，对方)的墙？
		pii ray_trace_dir(const Coord &pos, const Coord &dire);
	
	protected:
		//跑一次从snkid所在位置到全图的bfs，会考虑蛇尾部的移动
		//【log功能暂未实现】
		void find_path_bfs(int snkid = -1);
		vector<int> bank_siz_list(int snkid = -1);//【可能有效率问题】，返回一个长度为100的数组，在寻路bfs中作为check_norm的bank参数
		//计算act score和safe score，会撞死的返回-100
		//【特判了len<=2的蛇】
		void scan_act();
		void scan_act_bfs(int actid);
		double find_head(int nx, int ny);
		//计算“谦让值”与“攻击值”
		//某一个act将队友的“气”挤压到小于2，则polite_score减小；若是对手，则attack_score加大
		void calc_P_A_score();

		map_int_t dist_arr[8],path_arr[8];
		map_int_t temp_map_int;

	private:
		AI &ai;
		const Context &ctx;
		//snakes : "list[Snake]"
		const Snake &this_snake;
		Logger &logger;
		const static int x_leng = MAP_LENGTH;
		const static int y_leng = MAP_LENGTH;
		const int snkid;
		const Coord pos;
		const int camp;
		const int turn;

		//find_path系列常数
		const static int BANK_SIZ_LIST_SIZE = 100;
		//scan_act系列常数
		const static int SCAN_ACT_MAX_DEPTH = 6;
		constexpr static double SCAN_ACT_REDUCE_FACTOR[2] = {0.2,0.6};//敌,我
		constexpr static double CRIT_AIR_PARAM[5] = {4.5,0.75,-8.0,-1.0,0.5};//(上限绝对值,上限比例,惩罚绝对值,惩罚leng系数,奖励score系数)
		// constexpr static double LOW_AIR_PARAM[4] = {10,2.5,-0.8,0.2};//(上限绝对值,上限比例,惩罚系数,奖励score系数)
		//PA分系列常数
		// constexpr static double A_SMALL_BONUS[3] = {0,1.2,1.5};
		constexpr static double P_1_AIR_PARAM[2] = {-4,-0.4};
		constexpr static double P_NO_AIR_PARAM[2] = {-8,-1};
		constexpr static double A_AIR_MULT[2] = {0.7,0.3};//(0 air,1 air)
		constexpr static double A_ENV_BONUS[2] = {2,2};//(No laser, 4 snakes)
		
		//寻路系列常数
		
		constexpr static double GREEDY_DIR_SCORE = 4.0;
		constexpr static double FNDPTH_DIR_SCORE = 6.0;
		constexpr static double EM_RAY_COST[2] = {2,1.0/3};
		constexpr static double EM_SOLID_EFF = 0.75;
		const static int EM_SOLID_COUNT_MIN = 3;
};

//Search类
const int SNK_SIMU_TYPE_MAX = 128;
//【snklst一定要包括snkid】【不允许在turn=513时调用search】
//【snkid不是当前行动的蛇导致的后果暂不明确】
class Search {
	public:
		Search(const Context& ctx0, Logger& logger, const vector<int>& snklst, int snkid);

		void setup_search(int max_turn, double (*value_func)(const Context& ctx));
		//发起局部搜索
		//返回的act_score_t中的actid【不是ACT的下标】
		act_score_t search();
		

	private:
		Logger& logger;
		Context ctx0;
		const int camp;
		const int snkid;
		int max_turn, end_turn;
		bool snk_simu_type[SNK_SIMU_TYPE_MAX];
		double (*value_func)(const Context& ctx);

		vector<Context> dfs_stack;
		//局部搜索，采用模拟递归的方式进行，返回(走法,最大val)
		act_score_t search_dfs(int stack_ind);
		act_score_t search_comp(const act_score_t &a, const act_score_t &b, bool max_layer);

		int debug_search_cnt = 0;
};
Search::Search(const Context& ctx0, Logger& logger, const vector<int>& snklst, int snkid) :
ctx0(ctx0), logger(logger), camp(ctx0.current_player()), snkid(snkid)
{
	for(int i = 0; i < SNK_SIMU_TYPE_MAX; i++) snk_simu_type[i] = true;//所有id都在模拟范围内（包括未来的id）
	
	vector<int> snk_list;
	for(auto it = this->ctx0.my_snakes().begin(); it != this->ctx0.my_snakes().end(); it++) snk_list.push_back(it->id);
	for(auto it = this->ctx0.opponents_snakes().begin(); it != this->ctx0.opponents_snakes().end(); it++) snk_list.push_back(it->id);
	for(auto it = snk_list.begin(); it != snk_list.end(); it++) {
		bool keep = false;
		for(int j = 0; j < snklst.size(); j++) {
			if(snklst[j] == *it) {
				keep = true;
				break;
			}
		}
		if(!keep) snk_simu_type[*it] = false;//,printf("remove:%d\n",*it);//排除id
	}
}
void Search::setup_search(int max_turn, double (*value_func)(const Context& ctx)) {
	this->max_turn = max_turn;
	this->value_func = value_func;
	this->end_turn = min(this->ctx0.current_round() + this->max_turn,this->ctx0.max_round());//【最后一回合可能搜不完全】
	printf("setup: start_turn:%d max_turn:%d end_turn:%d\n",this->ctx0.current_round(),this->max_turn,this->end_turn);
}
act_score_t Search::search() {
	this->debug_search_cnt = 0;
	this->dfs_stack.push_back(ctx0);

	if(this->ctx0.current_round() > this->ctx0.max_round()) return ACT_SCORE_NULL;

	act_score_t ans = this->search_dfs(0);
	printf("searched:%d\n",this->debug_search_cnt);
	return ans;
}
act_score_t Search::search_dfs(int stack_ind) {
	act_score_t ans = ACT_SCORE_NULL;
	const Context& now = this->dfs_stack[stack_ind];
	const bool max_layer = (now.current_player() == this->camp);
	// printf("dfs%5d dep:%d snkid:%d\n",this->debug_search_cnt,stack_ind,now._current_snake_id);
	this->logger.log(0,"dfs%5d dep:%d snkid:%d",this->debug_search_cnt,stack_ind,now._current_snake_id);

	assert(now.current_round() <= this->end_turn);
	for(int i = 1; i <= ACT_MAXV; i++) {
		if(this->dfs_stack.size() == stack_ind+1) this->dfs_stack.push_back(this->dfs_stack[stack_ind]);
		else {
			while(this->dfs_stack.size() > stack_ind+1) this->dfs_stack.pop_back();
			this->dfs_stack.push_back(this->dfs_stack[stack_ind]);
		}
		
		if(this->dfs_stack[stack_ind+1].do_operation(Operation({i}))) {
			// printf("dfs%5d turn:%d dep:%d snkid:%d op:%d my_size:%d reg_siz%d\n",this->debug_search_cnt,now.current_round(),stack_ind,this->dfs_stack[stack_ind+1]._current_snake_id,i,this->dfs_stack[stack_ind+1].snake_list_0().size(),this->dfs_stack[stack_ind].snake_list_0().size());
			this->debug_search_cnt++;
			bool end = false;
			const Context& next = this->dfs_stack[stack_ind+1];
			if(now.current_round() == this->end_turn && now._current_snake_id == this->snkid) end = true;//到时间结束
			if(!next.inlist(this->snkid)) end = true;//蛇死
			if(end) {//不再扩展
				ans = this->search_comp(ans,act_score_t({i,(*this->value_func)(next)}),max_layer);
				continue;
			}
			
			while(!this->snk_simu_type[this->dfs_stack[stack_ind+1]._current_snake_id]) this->dfs_stack[stack_ind+1].skip_operation();
			// printf("skipped to:%d\n",this->dfs_stack[stack_ind+1]._current_snake_id);

			ans = this->search_comp(ans,act_score_t({i,this->search_dfs(stack_ind+1).val}),max_layer);
		}
	}
	return ans;
}
act_score_t Search::search_comp(const act_score_t &a, const act_score_t &b, bool max_layer) {//【不要全局域啊！】
	if(a == ACT_SCORE_NULL) return b;
	if(b == ACT_SCORE_NULL) return a;
	if(max_layer) {
		if(a.val >= b.val) return a;
		else return b;
	} else {
		if(a.val <= b.val) return a;
		else return b;
	}
}


//Ai类
bool tgt_alloc_cmp(const tgt_alloc_t &a, const tgt_alloc_t &b) {
	return a.cost < b.cost;//升序【可能不对】
}
AI::AI(Logger &logger) : 
logger(logger)
{
	
}
int AI::judge(const Snake &snake, const Context &ctx) {
	this->snake = &snake;
	this->ctx = &ctx;
	this->turn = ctx.current_round();
	this->logger.config(this->turn,snake.id);

	Assess &&assess_instance = Assess(*this,ctx,this->logger,snake.id);
	this->assess = &assess_instance;

	// Search sr((*this->ctx),this->logger,vector<int>({this->snake->id}),this->snake->id);
	// sr.setup_search(3,this->val_func0);
	// act_score_t res = sr.search();

	if(this->last_turn != ctx.current_round()) {
		this->last_turn = ctx.current_round();
		if(ctx.current_round() == 1) this->total_init();
		this->turn_control();
	}

	if(this->try_shoot()) return 5;//任务分配

	const int sol = this->try_solid();
	if(sol != -1) return sol+1;

	if(this->try_split()) {
		this->logger.log(1,"主动分裂，长度%d",this->snake->length());
		return 6;
	}
	return this->try_eat()+1;
}
void AI::total_init() {

}
void AI::turn_control() {
	this->assess->do_turn_assess();

	this->distribute_tgt();
}
void AI::distribute_tgt() {
	this->wanted_item.clear();
	for(int i = 0; i < ITEM_ALLOC_MAX; i++) this->item_alloc[i] = -1;
	for(auto it = this->ctx->my_snakes().begin(); it != this->ctx->my_snakes().end(); it++) this->wanted_item[it->id] = NULL_ITEM;

	vector<tgt_alloc_t> tgt_list;
	for(auto _item = this->ctx->item_list().begin(); _item != this->ctx->item_list().end(); _item++) {
		if(_item->eaten || _item->expired || this->assess->check_item_captured_team(*_item) != -1) continue;//排除已被吃/将被吃
		if(_item->time - this->turn > this->ALLOC_FTR_LIMIT) continue;//太过久远

		for(auto _friend = this->ctx->my_snakes().begin(); _friend != this->ctx->my_snakes().end(); _friend++) {
			const int dst = (*this->assess->dist_map[_friend->id])[_item->x][_item->y];
			if(dst == -1 || this->turn+dst >= _item->time+ITEM_EXPIRE_LIMIT) continue;

			const spd_map_t &fastest = this->assess->tot_spd[_item->x][_item->y];
			if(dst - fastest.dist > this->ALLOC_COMPETE_LIMIT) continue;//抢不过就不抢

			const int snkid = _friend->id;
			double cost = max(dst,_item->time - this->turn);//max(空间,时间)
			if(fastest.snkid != snkid) cost += this->ALLOC_SLOW_COST_PENA * (dst-fastest.dist);
			if(_item->type == 0) cost -= this->ALLOC_APPLE_SIZ_GAIN * _item->param;
			else {
				cost -= this->ALLOC_APPLE_SIZ_GAIN * this->ALLOC_LASER_AS_APPLE;
				cost += this->ALLOC_HAS_LASER_PENA * int(this->assess->has_laser(snkid));
			}

			if(cost <= this->ALLOC_MAX_COST_BOUND) tgt_list.push_back(tgt_alloc_t({snkid,cost,*_item}));
		}
	}
	sort(tgt_list.begin(),tgt_list.end(),tgt_alloc_cmp);

	int complete_cnt = 0;
	for(auto it = tgt_list.begin(); it != tgt_list.end(); it++) {
		if(complete_cnt >= this->ctx->my_snakes().size()) break;
		if(this->item_alloc[it->item.id] != -1 || this->wanted_item[it->snkid] != NULL_ITEM) continue;

		this->logger.log(1,"目标分配:蛇%2d -> %s 代价%.1f",it->snkid,it->item.to_string().c_str(),it->cost);
		this->item_alloc[it->item.id] = it->snkid;
		this->wanted_item[it->snkid] = it->item;
	}
}
bool AI::try_split() {
	// this->logger.log(0,"尾气:%d sp:%d",this->assess->calc_snk_air(this->snake->coord_list.back()),this->assess->can_split());
	if(!this->assess->can_split() || this->assess->calc_snk_air(this->snake->coord_list.back()) < 2) return false;
	if(this->snake->length() > 15 && this->ctx->my_snakes().size() < 4) return true;
	if(this->snake->length() > 13 && this->ctx->my_snakes().size() < 3) return true;
	if(this->snake->length() > 11 && this->ctx->my_snakes().size() < 2) return true;

	double max_safe_score = -1000;
	for(int i = 0; i < ACT_LEN; i++) max_safe_score = max(max_safe_score,this->assess->safe_score[i]);
	if(max_safe_score < this->SPLIT_THRESH) return true;

	return false;
}
bool AI::try_shoot() {
	if(!this->assess->can_shoot()) return false;
	const pii &ass = this->assess->ray_trace();
	if(ass.second - ass.first >= 2) {
		this->logger.log(1,"发射激光，击毁(%d,%d)",ass.first,ass.second);
		return true;
	}
	return false;
}
int AI::try_solid() {
	// double score = 0;

	const pii best_sol = this->assess->get_enclosing_leng();//利用率（前后期）
	if(best_sol.first == -1) return -1;
	
	double max_act_score = -1000;
	for(int i = 0; i < ACT_LEN; i++) max_act_score = max(max_act_score,this->assess->act_score[i]);
	if(max_act_score < 5) {
		this->logger.log(1,"主动固化，max_act_score%.1f",max_act_score);
		return best_sol.second;
	}
	return -1;

	// double max_safe_score = -1000;
	// for(int i = 0; i < ACT_LEN; i++) max_safe_score = max(max_safe_score,this->assess->safe_score[i]);
	// if(max_safe_score > this->SOL_SAFE_THRESH || this->ctx->my_snakes().size() < this->SOL_COUNT_MIN) return -1;
	// score += this->SOL_SAFE_THRESH - max_safe_score;

	// if(this->ctx->my_snakes().size() == SNAKE_LIMIT) {
	// 	score += this->SOL_HI_SNAKE_BONUS;
	// 	int longest = -100;
	// 	for(auto it = this->ctx->my_snakes().begin(); it != this->ctx->my_snakes().end(); it++) longest = max(longest,int(it->length()));
	// 	if(longest > this->SOL_LONG_SNK_PARAM[0]) score += this->SOL_LONG_SNK_PARAM[1] * (longest - this->SOL_LONG_SNK_PARAM[0]);
	// }

	// if(score >= this->SOL_SCORE_THRESH) {
	// 	this->logger.log(1,"主动固化，score%.1f",score);
	// 	return best_sol.second;
	// }
}
int AI::try_eat() {
	if(this->wanted_item[this->snake->id] == NULL_ITEM) {
		this->logger.log(1,"未分配到目标");
		return this->assess->random_step();
	}
	const Item &item = this->wanted_item[this->snake->id];
	return this->assess->find_path(Coord({item.x,item.y}));
}
double AI::val_func0(const Context& ctx) {
	return -100;
}

//Assess类
bool act_score_cmp(const act_score_t &a, const act_score_t &b) {
	return a.val > b.val;//降序【可能不对】
}
Assess::Assess(AI &ai, const Context &ctx, Logger &logger, int snkid) :
ai(ai),
ctx(ctx),
this_snake(ctx.find_snake(snkid)),
logger(logger),
snkid(snkid),
pos(ctx.find_snake(snkid)[0]),
camp(ctx.current_player()),
turn(ctx.current_round())
{
	this->do_snake_assess();
}
void Assess::do_turn_assess() {
	this->calc_spd_map();
}
void Assess::do_snake_assess() {
	this->refresh_all_bfs();
	this->calc_mixed_score();
}
void Assess::calc_mixed_score() {
	this->scan_act();
	this->calc_P_A_score();
	for(int i = 0; i < ACT_LEN; i++) this->mixed_score[i] = this->safe_score[i] + this->polite_score[i] + this->attack_score[i];
}
void Assess::refresh_all_bfs() {
	int alloc_ind = 0;
	this->dist_map.clear();
	this->path_map.clear();
	for(auto it = this->ctx.snake_list_0().begin(); it != this->ctx.snake_list_0().end(); it++) {
		dist_map[it->id] = &(this->dist_arr[alloc_ind]);
		path_map[it->id] = &(this->path_arr[alloc_ind]);
		alloc_ind++;
		this->find_path_bfs(it->id);
	}
	for(auto it = this->ctx.snake_list_1().begin(); it != this->ctx.snake_list_1().end(); it++) {
		dist_map[it->id] = &(this->dist_arr[alloc_ind]);
		path_map[it->id] = &(this->path_arr[alloc_ind]);
		alloc_ind++;
		this->find_path_bfs(it->id);
	}
}
void Assess::calc_spd_map() {
	for(int x = 0; x < this->x_leng; x++) {
		for(int y = 0; y < this->y_leng; y++) {
			friend_spd[x][y] = SPD_MAP_NULL;
			enemy_spd[x][y] = SPD_MAP_NULL;
			tot_spd[x][y] = SPD_MAP_NULL;
		}
	}
	for(int x = 0; x < this->x_leng; x++) {
		for(int y = 0; y < this->y_leng; y++) {
			int snkid = this->ctx.snake_map()[x][y];
			if(snkid != -1) {
				if(this->ctx.find_snake(snkid).camp == this->camp) friend_spd[x][y] = spd_map_t({0,snkid});
				else enemy_spd[x][y] = spd_map_t({0,snkid});
			}

			for(auto it = this->ctx.my_snakes().begin(); it != this->ctx.my_snakes().end(); it++) {
				int dst = (*this->dist_map[it->id])[x][y];
				if(dst == -1) continue;
				if(this->friend_spd[x][y] == SPD_MAP_NULL) this->friend_spd[x][y] = spd_map_t({dst,it->id});
				else if(this->friend_spd[x][y].dist > dst) this->friend_spd[x][y] = spd_map_t({dst,it->id});
			}
			for(auto it = this->ctx.opponents_snakes().begin(); it != this->ctx.opponents_snakes().end(); it++) {
				int dst = (*this->dist_map[it->id])[x][y];
				if(dst == -1) continue;
				if(this->enemy_spd[x][y] == SPD_MAP_NULL) this->enemy_spd[x][y] = spd_map_t({dst,it->id});
				else if(this->friend_spd[x][y].dist > dst) this->enemy_spd[x][y] = spd_map_t({dst,it->id});
			}
		}
	}
	for(int x = 0; x < this->x_leng; x++) {
		for(int y = 0; y < this->y_leng; y++) {
			int valf = this->friend_spd[x][y].dist,vale = this->enemy_spd[x][y].dist;
			if(valf == -1) valf = 1000;
			if(vale == -1) vale = 1000;
			if(min(valf,vale) == 1000) continue;
			if(valf <= vale) this->tot_spd[x][y] = this->friend_spd[x][y];
			else this->tot_spd[x][y] = this->enemy_spd[x][y];
		}
	}
}
void Assess::scan_act() {
	const double LOW_AIR_PARAM[4] = {10,2.5,-0.8,0.2};
	bool will_log = false;
	const int x = this->pos.x,y = this->pos.y;

	for(int i = 0; i < ACT_LEN; i++) this->act_score[i] = this->safe_score[i] = 0.0;
	for(int i = 0; i < ACT_LEN; i++) {
		const int tx = x+ACT[i][0],ty = y+ACT[i][1];
		if(!this->check_nstep_norm(tx,ty)) {
			this->act_score[i] = this->safe_score[i] = -200.0;
			continue;
		}
		if(this->this_snake.length() <= 2) {
			this->act_score[i] = 25.0;
			continue;
		}
		this->scan_act_bfs(i);

		const double leng = this->this_snake.length() + max(this->this_snake.length_bank,2);
		if(this->act_score[i] <= this->CRIT_AIR_PARAM[0] || this->act_score[i] <= leng*(this->CRIT_AIR_PARAM[1]))
			this->safe_score[i] += this->CRIT_AIR_PARAM[2] + leng*this->CRIT_AIR_PARAM[3] + this->act_score[i]*this->CRIT_AIR_PARAM[4];
		else if(this->act_score[i] <= LOW_AIR_PARAM[0] || this->act_score[i] <= min(leng*(LOW_AIR_PARAM[1]),20.0))
			this->safe_score[i] += min(0.0,(max(LOW_AIR_PARAM[0],leng*(LOW_AIR_PARAM[1]))-this->act_score[i])*LOW_AIR_PARAM[2] + this->act_score[i]*LOW_AIR_PARAM[3]);
		
		if(this->safe_score[i] > -90 && this->safe_score[i] < -0.5) will_log = true;
	}

	if(will_log) {
		this->logger.log(0,"act_score:[%.1f,%.1f,%.1f,%.1f]",this->act_score[0],this->act_score[1],this->act_score[2],this->act_score[3]);
		this->logger.log(0,"safe_score:[%.1f,%.1f,%.1f,%.1f]",this->safe_score[0],this->safe_score[1],this->safe_score[2],this->safe_score[3]);
	}
}
void Assess::scan_act_bfs(int actid) {
	const int rx = this->pos.x,ry = this->pos.y;

	for(int x = 0; x < this->x_leng; x++) for(int y = 0; y < this->y_leng; y++) this->temp_map_int[x][y] = 0;
	this->temp_map_int[rx][ry] = 1,this->temp_map_int[rx+ACT[actid][0]][ry+ACT[actid][1]] = 1;

	queue<scan_act_q_t> qu;//x,y,step,val
	qu.push(scan_act_q_t({rx+ACT[actid][0],ry+ACT[actid][1],1,1.0}));
	while(qu.size()) {
		const scan_act_q_t now = qu.front();
		qu.pop();
		if(now.step > this->SCAN_ACT_MAX_DEPTH) continue;

		for(int i = 0; i < ACT_LEN; i++) {
			const int tx = now.x+ACT[i][0],ty = now.y+ACT[i][1];
			if(!this->check_nstep_norm(tx,ty,now.step+1) || temp_map_int[tx][ty]) continue;
			const double heads = this->find_head(tx,ty);
			this->temp_map_int[tx][ty] = 1;
			this->act_score[actid] += now.val*heads;
			qu.push(scan_act_q_t({tx,ty,now.step+1,now.val*heads}));
		}
	}
}
double Assess::find_head(int nx, int ny) {
	double ans = 1;
	for(int i = 0; i < ACT_LEN; i++) {
		int tx = nx + ACT[i][0],ty = ny + ACT[i][1];
		if(tx < 0 || ty < 0 || tx >= 16 || ty >= 16) continue;

		int snk = this->ctx.snake_map()[tx][ty];//不是自己的头
		if(snk != -1 && snk != this->snkid && Coord({tx,ty}) == this->ctx.find_snake(snk)[0]) {
			if(this->ctx.find_snake(snk).camp == this->camp) ans *= this->SCAN_ACT_REDUCE_FACTOR[1];
			else ans *= this->SCAN_ACT_REDUCE_FACTOR[0];
		}
	}
	return ans;
}
void Assess::calc_P_A_score() {
	const double A_SMALL_BONUS[3] = {0,1.2,1.5};
	const int x = this->pos.x, y = this->pos.y;
	bool will_log_atk = false, will_log_pol = false;
	for(int i = 0; i < ACT_LEN; i++) this->attack_score[i] = this->polite_score[i] = -100;

	for(int i = 0; i < ACT_LEN; i++) {
		const int tx = x+ACT[i][0], ty = y+ACT[i][1];
		if(!this->check_nstep_norm(tx,ty)) continue;
		this->attack_score[i] = this->polite_score[i] = 0;

		Coord extra_go = Coord({-1,-1});
		if(!this->this_snake.length_bank) extra_go = this->this_snake.coord_list.back();
		for(auto _friend = this->ctx.my_snakes().begin(); _friend != this->ctx.my_snakes().end(); _friend++) {
			if(_friend->id == this->snkid) continue;
			const int leng = _friend->length() + _friend->length_bank;
			const int curr = this->calc_snk_air(_friend->coord_list[0]);
			const int ftr = this->calc_snk_air(_friend->coord_list[0],Coord({tx,ty}),extra_go);
			if(ftr < curr && ftr == 1) this->polite_score[i] += this->P_1_AIR_PARAM[0] + leng*this->P_1_AIR_PARAM[1];
			if(ftr < curr && ftr == 0) this->polite_score[i] += this->P_NO_AIR_PARAM[0] + leng*this->P_NO_AIR_PARAM[1];
		}
		for(auto _enemy = this->ctx.opponents_snakes().begin(); _enemy != this->ctx.opponents_snakes().end(); _enemy++) {
			const int leng = _enemy->length() + _enemy->length_bank;
			const int curr = this->calc_snk_air(_enemy->coord_list[0]);
			const int ftr = this->calc_snk_air(_enemy->coord_list[0],Coord({tx,ty}),extra_go);
			if(ftr < curr && ftr == 1) {
				this->logger.log(0,"atk响应: id:%2d curr,ftr:%d,%d coord:%d,%d",_enemy->id,curr,ftr,_enemy->coord_list[0].x,_enemy->coord_list[0].y);
				this->attack_score[i] += leng*this->A_AIR_MULT[1];
			}
			if(ftr < curr && ftr == 0) {
				this->logger.log(0,"atk响应: id:%2d curr,ftr:%d,%d",_enemy->id,curr,ftr);
				this->attack_score[i] += leng*this->A_AIR_MULT[0];
				if(!this->can_shoot(_enemy->id)) this->attack_score[i] += this->A_ENV_BONUS[0];
				if(!this->can_split(_enemy->id)) this->attack_score[i] += this->A_ENV_BONUS[1];
			}
		}
		if(this->attack_score[i] > 0.5) will_log_atk = true;
		if(this->polite_score[i] > -90 && this->polite_score[i] < -0.5) will_log_pol = true;
	}
	const int leng = this->this_snake.length() + this->this_snake.length_bank;
	if(leng <= 2) for(int i = 0; i < ACT_LEN; i++) this->attack_score[i] *= A_SMALL_BONUS[leng];

	if(will_log_atk) this->logger.log(0,"atk_score:[%.1f,%.1f,%.1f,%.1f]",this->attack_score[0],this->attack_score[1],this->attack_score[2],this->attack_score[3]);
	if(will_log_pol) this->logger.log(0,"pol_score:[%.1f,%.1f,%.1f,%.1f]",this->polite_score[0],this->polite_score[1],this->polite_score[2],this->polite_score[3]);
}
void Assess::find_path_bfs(int snkid) {
	bool will_log = false;
	if(snkid == -1) snkid = this->snkid;
	const vector<int> &&bank_list = this->bank_siz_list(snkid);
	const int nx = this->ctx.find_snake(snkid)[0].x,ny = this->ctx.find_snake(snkid)[0].y;

	map_int_t *pth_map = this->path_map[snkid],*dst_map = this->dist_map[snkid];
	for(int x = 0; x < this->x_leng; x++) for(int y = 0; y < this->y_leng; y++) (*pth_map)[x][y] = (*dst_map)[x][y] = -1;
	(*pth_map)[nx][ny] = -1,(*dst_map)[nx][ny] = 0;

	queue<find_path_q_t> qu;
	qu.push(find_path_q_t({nx,ny,0}));
	while(qu.size()) {
		const find_path_q_t now = qu.front();
		qu.pop();

		if(now.step > 45) will_log = true;

		for(int i = 0; i < ACT_LEN; i++) {
			const int tx = now.x+ACT[i][0],ty = now.y+ACT[i][1];
			if(!this->check_nstep_norm(tx,ty,now.step+1,snkid,bank_list[now.step])) continue;
			if((*dst_map)[tx][ty] == -1) {
				qu.push(find_path_q_t({tx,ty,now.step+1}));
				(*pth_map)[tx][ty] = i;
				(*dst_map)[tx][ty] = now.step+1;
			}
		}
	}

	// if(will_log) this->logger.log(0,)
}
vector<int> Assess::bank_siz_list(int snkid) {
	vector<int> ans;
	if(snkid == -1) snkid = this->snkid;

	const vector<Item> &&item_list = this->get_captured_items(snkid,0);
	ans.push_back(this->ctx.find_snake(snkid).length_bank);
	for(int i = 1; i < this->BANK_SIZ_LIST_SIZE; i++) {
		ans.push_back(ans[i-1]);
		for(auto it = item_list.begin(); it != item_list.end(); it++) if(it->time == this->turn + i) ans[i] += it->param;
	}
	return ans;
}
inline int Assess::rev_step(int st) {
	const int RP[4] = {2,3,0,1};
	return RP[st];
}
int Assess::random_step() {
	act_score_t random_list[4];
	for(int i = 0; i < ACT_LEN; i++) random_list[i] = act_score_t({i,this->mixed_score[i]});
	sort(&random_list[0],&random_list[ACT_LEN],act_score_cmp);

	if(random_list[0].val < -80) return this->emergency_handle();
	this->logger.log(1,"随机漫步:%d",random_list[0].actid);
	return random_list[0].actid;
}
int Assess::greedy_step(const Coord &tgt) {
	act_score_t greedy_list[4];
	const int x = this->pos.x, y = this->pos.y;
	const int dx = tgt.x-x, dy = tgt.y-y;

	for(int i = 0; i < ACT_LEN; i++) greedy_list[i] = act_score_t({i,this->mixed_score[i]});
	if(dx > 0) greedy_list[0].val += this->GREEDY_DIR_SCORE;
	if(dx < 0) greedy_list[2].val += this->GREEDY_DIR_SCORE;
	if(dy > 0) greedy_list[1].val += this->GREEDY_DIR_SCORE;
	if(dy < 0) greedy_list[3].val += this->GREEDY_DIR_SCORE;
	sort(&greedy_list[0],&greedy_list[ACT_LEN],act_score_cmp);

	if(greedy_list[0].val < -80) return this->emergency_handle();
	this->logger.log(1,"贪心寻路:%d",greedy_list[0].actid);
	return greedy_list[0].actid;
}
int Assess::find_path(const Coord &tgt) {
	int rev = -1;
	int x = tgt.x, y = tgt.y;
	if(tgt == this->pos) return this->random_step();
	if((*this->path_map[this->snkid])[x][y] == -1) return this->greedy_step(tgt);

	act_score_t bfs_list[4];
	while(x != this->pos.x || y != this->pos.y) {
		if((*this->path_map[this->snkid])[x][y] == -1) assert(false);
		rev = this->rev_step((*this->path_map[this->snkid])[x][y]);
		x += ACT[rev][0], y += ACT[rev][1];
	}
	for(int i = 0; i < ACT_LEN; i++) bfs_list[i] = act_score_t({i,this->mixed_score[i]});
	bfs_list[this->rev_step(rev)].val += this->FNDPTH_DIR_SCORE;
	sort(&bfs_list[0],&bfs_list[ACT_LEN],act_score_cmp);

	if(bfs_list[0].actid != this->rev_step(rev))
		this->logger.log(1,"寻路[非原向]:%d 目标:(%2d,%2d)",bfs_list[0].actid,tgt.x,tgt.y);
	else this->logger.log(1,"寻路:%d 目标:(%2d,%2d)",bfs_list[0].actid,tgt.x,tgt.y);
	return bfs_list[0].actid;
}
int Assess::emergency_handle() {
	//先尝试发激光
	if(this->can_shoot()) {
		pii &&rt = this->ray_trace();
		if(rt.first-rt.second <= this->EM_RAY_COST[0] && rt.first-rt.second <= this->this_snake.length()*this->EM_RAY_COST[1]) {
			this->logger.log(1,"紧急处理:发射激光，击毁(%d,%d)",rt.first,rt.second);
			return 5 - 1;
		}
	}
	//找到不犯规的走法
	vector<int> valid;
	const pii &best = this->get_enclosing_leng();
	for(int i = 0; i < ACT_LEN; i++) {
		const int tx = this->pos.x+ACT[i][0], ty = this->pos.y+ACT[i][1];
		if(tx < 0 || ty < 0 || tx >= 16 || ty >= 16) valid.push_back(i);
		else if(this->ctx.snake_map()[tx][ty] != this->snkid) valid.push_back(i);
	}
	//尝试固化（蛇）
	if(best.first >= this->EM_SOLID_EFF*(this->this_snake.length()+this->this_snake.length_bank) && this->ctx.my_snakes().size() >= this->EM_SOLID_COUNT_MIN) {
		this->logger.log(1,"紧急处理:主动固化，利用%d格蛇长",best.first);
		this->release_target();
		return best.second;
	}
	//尝试分裂
	if(this->can_split()) {
		this->logger.log(1,"紧急处理:分裂");
		return 6 - 1;
	}
	//都不行，随便撞
	if(best.first < 0) {
		this->logger.log(1,"紧急处理:%d(防倒车)",valid[0]);
		this->release_target();
		return valid[0];
	}
	this->logger.log(1,"紧急处理:被动固化，利用%d格蛇长",best.first);
	this->release_target();
	return best.second;
}
void Assess::release_target(int snkid) {
	if(snkid == -1) snkid = this->snkid;
	const Item &tgt = this->ai.wanted_item[snkid];
	if(tgt != NULL_ITEM) {
		this->logger.log(1,"释放原目标%s",tgt.to_string().c_str());
		this->ai.item_alloc[tgt.id] = -1;
	}
}
vector<Item> Assess::get_captured_items(int snkid, int item_tp) {
	vector<Item> ans;
	if(snkid == -1) snkid = this->snkid;
	for(auto it = this->ctx.item_list().begin(); it != this->ctx.item_list().end(); it++) {
		if(item_tp != -1 && it->type != item_tp) continue;
		if(it->time - this->turn > this->ctx.find_snake(snkid).length() + this->ctx.find_snake(snkid).length_bank) continue;
		if(this->check_item_captured((*it),snkid)) ans.push_back((*it));
	}
	return ans;
}
int Assess::check_item_captured_team(const Item &item) {
	const int x = item.x,y = item.y;
	const int snkid = this->ctx.snake_map()[x][y];

	if(snkid == -1) return -1;
	if(this->check_item_captured(item,snkid)) return this->ctx.find_snake(snkid).camp;
	return -1;
}
bool Assess::check_item_captured(const Item &item, int snkid) {
	const int x = item.x,y = item.y;
	const Coord crd = Coord({x,y});
	if(snkid == -1) snkid = this->snkid;

	if(this->ctx.snake_map()[x][y] != snkid) return false;

	const Snake &snk = this->ctx.find_snake(snkid);
	for(int i = 0; i < snk.length(); i++) {
		if(snk[i] == crd) {
			if(item.time - this->ctx.current_round() < snk.length()-i + snk.length_bank) return true;
			return false;
		}
	}
	assert(false);
}
int Assess::get_pos_on_snake(const Coord &pos) {
	const int x = pos.x,y = pos.y;
	const Snake &snk = this->ctx.find_snake(this->ctx.snake_map()[x][y]);
	for(int i = 0; i < snk.length(); i++) if(snk[i] == pos) return snk.length()-i;
	assert(false);
}
int Assess::calc_snk_air(const Coord &pos,const Coord &extra_block,const Coord &extra_go) {
	int ans = 0;
	const int x = pos.x, y = pos.y;
	const int snkid = this->ctx.snake_map()[x][y];
	// this->logger.log(0,"calc_snk_air called with (%d,%d) (%d,%d) (%d,%d) x=%d,y=%d",pos.x,pos.y,extra_block.x,extra_block.y,extra_go.x,extra_go.y,x,y);

	for(int i = 0; i < ACT_LEN; i++) {
		const int tx = x+ACT[i][0], ty = y+ACT[i][1];
		const Coord t_pos = Coord({tx,ty});
		if(t_pos == extra_block) continue;
		if(this->check_nstep_norm(tx,ty,1,snkid)) ans++;
		else if(t_pos == extra_go) ans++;
	}
	return ans;
}
pii Assess::get_enclosing_leng(int snkid) {
	if(snkid == -1) snkid = this->snkid;
	pii best = pii({-1,-1});
	const Snake &snk = this->ctx.find_snake(snkid);
	for(int i = 0; i < ACT_LEN; i++) {
		const int tx = snk[0].x+ACT[i][0], ty = snk[0].y+ACT[i][1];
		if(tx < 0 || ty < 0 || tx >= 16 || ty >= 16) continue;
		if(this->ctx.snake_map()[tx][ty] != snkid) continue;
		if(Coord({tx,ty}) == snk.coord_list.back() && snk.length_bank == 0) continue;
		if(snk.length() > 2 && tx == snk[1].x && ty == snk[1].y) continue;
		if(snk.length() == 2 && tx == snk[1].x && ty == snk[1].y && snk.length_bank > 0) continue;

		const int leng = snk.length() - this->get_pos_on_snake(Coord({tx,ty}));
		if(leng > best.first) best = pii({leng,i});
	}
	return best;
}
bool Assess::check_first(int first, int second) {
	bool found_now = false;
	for(auto it = this->ctx.snake_list_0().begin(); it != this->ctx.snake_list_0().end(); it++) {//相当于从snkid搜索到结尾
		if(it->id == this->snkid) found_now = true;
		if(found_now && it->id == first) return true;
		if(found_now && it->id == second) return false;
	}
	for(auto it = this->ctx.snake_list_1().begin(); it != this->ctx.snake_list_1().end(); it++) {
		if(it->id == this->snkid) found_now = true;
		if(found_now && it->id == first) return true;
		if(found_now && it->id == second) return false;
	}

	for(auto it = this->ctx.snake_list_0().begin(); it != this->ctx.snake_list_0().end(); it++) {//相当于从开头搜索到snkid
		if(found_now && it->id == first) return true;
		if(found_now && it->id == second) return false;
	}
	for(auto it = this->ctx.snake_list_1().begin(); it != this->ctx.snake_list_1().end(); it++) {
		if(found_now && it->id == first) return true;
		if(found_now && it->id == second) return false;
	}
	assert(false);
}
bool Assess::check_nstep_norm(int tx, int ty, int step, int snkid, int self_bloc_bank_val) {
	if(snkid == -1) snkid = this->snkid;

	if(tx < 0 || ty < 0 || tx >= 16 || ty >= 16 || this->ctx.wall_map()[tx][ty] != -1) return false;//越界/撞墙

	const int blocking_snake = this->ctx.snake_map()[tx][ty];
	if(blocking_snake == -1) return true;

	//仅在self_blocking时采用override
	if(self_bloc_bank_val == -1) self_bloc_bank_val = this->ctx.find_snake(blocking_snake).length_bank;
	else if(blocking_snake != this->snkid) self_bloc_bank_val = this->ctx.find_snake(blocking_snake).length_bank;
	const Snake& blocking_snk = this->ctx.find_snake(blocking_snake);
	const int leave_time = this->get_pos_on_snake(Coord({tx,ty})) + self_bloc_bank_val;

	if(blocking_snake == snkid) {//self_blocking
		if(blocking_snk.length() > 2 && tx == blocking_snk[1].x && ty == blocking_snk[1].y) return false;//【感觉不会碰这样的问题？】
		if(blocking_snk.length() == 2 && tx == blocking_snk[1].x && ty == blocking_snk[1].y && blocking_snk.length_bank > 0) return false;
		if(leave_time <= step) return true;
		return false;
	}

	if(leave_time < step) return true;
	if(leave_time > step) return false;
	if(this->check_first(snkid,blocking_snake)) return false;
	return true;
}
bool Assess::can_split(int snkid) {
	if(snkid == -1) snkid = this->snkid;
	if(this->ctx.my_snakes().size() >= 4) return false;
	if(this->ctx.find_snake(snkid).length() < 2) return false;
	return true;
}
bool Assess::has_laser(int snkid) {
	if(snkid == -1) snkid = this->snkid;
	return this->ctx.find_snake(snkid).has_laser();
}
bool Assess::can_shoot(int snkid) {
	if(snkid == -1) snkid = this->snkid;
	if(!this->has_laser(snkid) || this->ctx.find_snake(snkid).length() < 2) return false;
	return true;
}
pii Assess::ray_trace(int snkid) {
	if(snkid == -1) snkid = this->snkid;
	const Snake &snk = this->ctx.find_snake(snkid);
	if(snk.length() < 2) return pii({-1,-1});

	const Coord pos0 = snk[0],pos1 = snk[1];
	return this->ray_trace_dir(pos0,Coord({pos0.x-pos1.x,pos0.y-pos1.y}));
}
pii Assess::ray_trace_dir(const Coord &pos, const Coord &dire) {
	int fr = 0,en = 0;
	int tx = pos.x,ty = pos.y;
	while(tx >= 0 && ty >= 0 && tx < 16 && ty < 16) {
		int wall = this->ctx.wall_map()[tx][ty];
		if(wall != -1) {
			if(wall == this->camp) fr++;
			else en++;
		}
		tx += dire.x,ty += dire.y;
	}
	return pii({fr,en});
}

//Search类

Logger logger = Logger();
AI ai = AI(logger);

Operation make_your_decision( const Snake &snake, const Context &ctx, const OpHistory& op_history) {
	const int op = ai.judge(snake,ctx);
	ai.logger.flush();
	return Operation({op});
}
void game_over( int gameover_type, int winner, int p0_score, int p1_score ) {
	fprintf( stderr, "%d %d %d %d", gameover_type, winner, p0_score, p1_score );
}
int main( int argc, char *argv[] ) {
	argc = min(3,argc);//【危】
	SnakeGoAI start( argc, argv );
}