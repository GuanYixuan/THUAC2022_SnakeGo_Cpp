#include <cassert>
#include <cstdarg>
#include <iostream>
#include <map>
#include "adk.hpp"
using namespace std;

//Logger类
const bool LOG_SWITCH = true;
const bool LOG_STDOUT = true;
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
	if(LOG_SWITCH) {
		if(LOG_STDOUT) file = stdout;
		else file = fopen("log.log","a");
	}
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
		fprintf(this->file,"turn%03d-%02d: %s\n",this->turn,this->snkid,this->buffer);
	}
}
void Logger::flush() {
	if(LOG_SWITCH) fflush(this->file);
}

//Ai类
const int ALLOC_MAX = 512;
struct item_alloc_t {//物品目标分配结构体
	int snkid;
	double cost;
	Item item;
};
bool item_alloc_cmp(const item_alloc_t &a, const item_alloc_t &b) { return a.cost < b.cost; }
struct kl_alloc_t {//KL任务分配结构体
	int tgt_id,killer_id;//必需参数
	int type;//0=kill_1,1=kill_2,2=limit_1,3=limit_2,4=help
	int status;//0=trying,1=ongoing
	int waller_id,turner_id;//-1=不需要，-9=暂无
	int wall_leng,turn_dis;
};
struct shoot_alloc_t {
	int gain,loss;
	Coord pos;
	int turn_dir;
};
struct target {
	//-1=None,0=Item,1=k/l,2=shoot,3=solid,4=dead_sig,5=command
	//每加一个type，都要在AI::release_target中提供支持
	int type;
	int time;

	bool isnull() const { return this->type == -1; }
};
const target NULL_TARGET {-1,-1};
struct target_kl {
	int tgt_id,killer_id;//必需参数
	int status,type;//0=trying,1=ongoing
	int waller_id,turner_id;//-1=不需要，-9=暂无
	inline bool isnull() const { return this->tgt_id == -1; }
	// inline bool need_waller() const {}
};
struct target_solid {
	int snkid;
	int long_side,short_side;
	int cover_size,remove_wall,require_length,tot_dist;
	Coord origin;
	vector<Coord> pos_list;
	vector<int> dist_list;
	bool in_error;

	bool operator<(const target_solid& other) const {//注意这不是小于号的定义
		return this->cover_size + this->remove_wall > other.cover_size + other.remove_wall;
	}
};
struct dead_signal {
	int snkid;
	int leng,bank,area_now;
	int time_left;
	bool laser;
};
class Assess;
class AI {
	public:
		AI(Logger &logger);
		Logger &logger;
		int judge(const Snake &snake, const Context &ctx);
		
		//key是snkid，val是此蛇当前的目标
		//如果没有目标，val应为常量NULL_TARGET
		map<int, target> target_list;
		map<int, int> item_addons;
		map<int, target_kl> kl_addons;
		map<int, shoot_alloc_t> shoot_addons;
		map<int, target_solid> solid_addons;
		map<int, dead_signal> dead_signal_addons;
		map<int, int> command_addons;

		void total_init();
		void turn_control();
		void turn_init();
		
		//安全区相关计算
		void calc_safe_area();
		inline int __get_area(int x, int y) { return (x>=8) + 2*(y>=8); }
		inline Coord __get_mid(int aid) { return Coord({(aid&1) ? 12 : 4, (aid&2) ? 12 : 4}); }
		inline int __get_dist(int aid, int snkid);
		int area_level[4];//1=空旷,2=一般,3=拥挤
		int area_occupy_val[4];
		int area_occupy_snk[4];
		bool area_alloc[4];
		Coord safe_area_alloc[ALLOC_MAX];

		void plan_dead_signal();
		int do_dead_signal();
		int do_command();
		vector<dead_signal> dead_signal_list;
		int dead_signal_alloc[ALLOC_MAX];
		int reserved_size = 0;


		//找到可能的kl机会，并更新kl_list
		void plan_kl();
		int do_kl();
		vector<kl_alloc_t> kl_list;

		//寻找融化墙壁任务
		void plan_shoot();
		int do_shoot();
		vector<shoot_alloc_t> shoot_list;
		int x_wall_alloc[MAP_LENGTH];
		int y_wall_alloc[MAP_LENGTH];

		//规划固化任务
		void plan_solid();
		void _check_solid_push(target_solid&& addon);
		int do_solid();
		vector<target_solid> solid_list;

		//[总控函数]为所有蛇分配目标
		//目前仅有食物目标（每回合自动刷新）
		void distribute_tgt();
		int do_eat();
		//index是item/snake的id，val是该item/snake属于的蛇的id
		int item_alloc[ALLOC_MAX];

		//随缘任务
		bool try_split();
		bool try_shoot();
		int try_solid();

		void release_target(int snkid = -1, bool silent = false);
		void publish_dead_signal();

	private:
		Assess* assess;
		int turn;
		const Context* ctx;
		const Snake* snake;

		int last_turn = -1;

		//安全区分配/疏散任务
		const static int SAREA_SHORT_LENG = 5;

		//KL目标分配系列函数
		const static int KL_TIME_LIM = 5;
		const static int KL_TGT_MIN_LENG = 6;
		const static int KL_TGT_ABA_MIN_LENG = 4;
		const static int KL_MAX_NORM_DIS = 2;
		const static int KL_MAX_HELP_DIS = 5;
		const static int KL_MAX_ALLY_LENG = 14;


		//物品目标分配系列常数
		const static int ITEM_FTR_LIMIT = 20;
		const static int ITEM_COMPETE_LIMIT = 6;
		constexpr static double ITEM_SLOW_COST_PENA = 1.5;
		constexpr static double ITEM_APPLE_SIZ_GAIN = 1.5;
		constexpr static double ITEM_LASER_AS_APPLE = 1;
		constexpr static double ITEM_HAS_LASER_PENA = 7;
		constexpr static double ITEM_MAX_COST_BOUND = 16;
		//随缘solidify系列常数
		const static int SOL_MIN_LENG = 16;
		constexpr static double SOL_DANG_EFFI = 0.85;
		constexpr static double SOL_DYING_EFFI = 0.75;
		constexpr static double SOL_IDLE_EFFI = 0.9;
		constexpr static double SOL_HIGH_EFFI = 1.2;
		constexpr static double SOL_SAFE_THRESH = -8;
		//随缘split系列常数
		constexpr static double SPLIT_THRESH = -12;
};


//Assess类
typedef pair<int,int> pii;
typedef int map_int_t[MAP_LENGTH][MAP_LENGTH];

struct act_score_t {
	int actid;
	double val;

	bool operator==(const act_score_t &b) const {return actid == b.actid && val == b.val;}
	bool operator!=(const act_score_t &b) const {return !((*this) == b);}
};
const act_score_t ACT_SCORE_NULL = act_score_t({-100,-1});
bool act_score_cmp(const act_score_t &a, const act_score_t &b) { return a.val > b.val; }
struct mix_score_t {//条目跟Assess里的score保持一致
	double mixed_score,search_score;
	double act_score,safe_score;
	double attack_score,polite_score;
};
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

		//测试区
		bool dead_signal = false;
		spd_map_t friend_spd[MAP_LENGTH][MAP_LENGTH];
		spd_map_t enemy_spd[MAP_LENGTH][MAP_LENGTH];
		spd_map_t tot_spd[MAP_LENGTH][MAP_LENGTH];
		double mixed_score[ACT_LEN],search_score[ACT_MAXV];
		double act_score[ACT_LEN],safe_score[ACT_LEN];
		double attack_score[ACT_LEN],polite_score[ACT_LEN];
		mix_score_t dir_scores[ACT_LEN];

		int get_bfs_dis(const Coord& tgt, int snkid = -1);
		map<int, map_int_t*> dist_map;//key是snkid
		map<int, map_int_t*> path_map;

		int area_occupy_val[4];

		//做一些只在每回合开始时要做的事
		//应该在do_snake_assess后调用
		void do_turn_assess();
		void do_snake_assess();
		//把attack/polite/safe/搜索分都算出来，并汇总到mixed_score里面
		//不会调用refresh_all_bfs
		void calc_mixed_score();
		//对所有蛇做一次bfs，并清除已死蛇的数据（会考虑即将吃到的食物）
		//这会更新dist_map,path_map
		void refresh_all_bfs();
		//用搜索对各个方向进行评分
		void mixed_search();
		//计算“速度势力图”，即哪方能先到达指定格
		//【默认蛇身/头所在格到达时间为0】
		void calc_spd_map();
		

		inline int rev_step(int st);
		//不设任何引导，根据mixed_score的最优值走一格
		int random_step();
		//设置一个贪心走向tgt的引导，根据它与mixed_score之和的最优值走一格
		int greedy_step(const Coord &tgt, double (*dir_assess)(int ind, bool directed, const mix_score_t& scores) = GRED_ASSESS_REGULAR);
		//根据find_path_bfs的结果设置一个走向tgt的引导，根据它与mixed_score之和的最优值走一格
		int find_path(const Coord &tgt, double (*dir_assess)(int ind, bool directed, const mix_score_t& scores) = DIR_ASSESS_REGULAR);
		static double DIR_ASSESS_SOLID(int ind, bool directed, const mix_score_t& scores);
		static double DIR_ASSESS_KL(int ind, bool directed, const mix_score_t& scores);
		//往"安全的"地方走一格
		int go_safe();
		//处理紧急情况
		int emergency_handle();

		//返回一个vector<Item>，其中是已被此蛇稳吃的所有Item，可手动限定type
		//【可能有效率问题】
		vector<Item> get_captured_items(int snkid = -1, int item_tp = -1);
		//检查物品是否被哪方的蛇占住了（可以用身子直接吃掉），没有则返回-1
		int check_item_captured_team(const Item &item);
		//检查物品是否已经被snkid的蛇占住了（可以用身子直接吃掉）
		bool check_item_captured(const Item &item, int snkid = -1);
		//返回pos格是蛇上的第几格，从尾部算起，需保证pos格有蛇
		int get_pos_on_snake(const Coord &pos);
		//计算当前位于pos的蛇头有几个方向可走，可添加一个额外堵塞位置extra_block及一个额外可行位置extra_go
		//【这里默认extra_block是你的蛇头而extra_go是蛇尾】
		int calc_snk_air(const Coord &pos,const Coord &extra_block = Coord({-1,-1}),const Coord &extra_go = Coord({-1,-1}));
		//计算snkid立刻主动进行固化能利用的最大身体长度，返回(最大长度,对应ACT下标)
        //如果无法进行固化，则返回(-1,-1)
		pii get_enclosing_leng(int snkid = -1);
		//计算【当前蛇】立刻主动进行固化能包围的最大区域面积，包含圈住的对手墙面积，返回(最大面积,对应ACT下标)
        //如果无法进行固化，则返回(-1,-1)
		pii get_enclosing_area();
		//检查snkid在最近leng格内是否走直线
		//对于leng>snkid.leng的情况，返回false
		bool check_go_straight(int snkid = -1, int leng = 3);
		//计算attacker走向target时的方向夹角
		//不会考虑中间的障碍物
		double get_encounter_angle(int attacker, int target);
		//获取snkid到达(x,y)及其相邻格所需的最短时间
		int get_adjcent_dis(int x, int y, int snkid = -1);
		//推算snkid沿当前方向继续走dist格所到达的坐标
		//注意：该函数对长度为1的蛇返回NULL_COORD，若结果超出地图范围也将返回NULL_COORD
		Coord trace_head_dir(int dist, int snkid = -1);
		//检查从start到end的道路是否通畅，会忽略编号为snkid的蛇身
		//注意：start至end间不能有转弯
		bool check_path_clear(const Coord& start, const Coord& end, int snkid = -1);
		//检查snkid能否立即在不犯规的情况下死亡(撞墙)
		//可以则返回actid，否则返回-1
		int find_dead(int snkid = -1);

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
		void __scan_act_bfs(int actid);
		double __calc_head_pena(int nx, int ny, int snkid);
		//计算“谦让值”与“攻击值”
		//某一个act将队友的“气”挤压到小于2，则polite_score减小；若是对手，则attack_score加大
		void calc_P_A_score();

		map_int_t dist_arr[8],path_arr[8];
		map_int_t temp_map_int;

	private:
		AI &ai;
		const Context &ctx;
		const Snake &this_snake;
		Logger &logger;
		const static int x_leng = MAP_LENGTH;
		const static int y_leng = MAP_LENGTH;
		const int snkid;
		const Coord pos;
		const int camp;
		const int turn;


		//mixed_search系列常数
		constexpr static double MIXED_SEARCH_WEIGHT = 2.0;
		// constexpr static int MIXED_SEARCH_DEPTH[9] = {0,7,4,2,2,1,1,1,1};
		constexpr static int MIXED_SEARCH_SNK_DIS_THRESH[2] = {4,4};//到头距离，(敌,我)
		static double mixed_val_func(const Context& begin, const Context& end, int snkid);
		static double attack_val_func(const Context& begin, const Context& end, int snkid);
		//find_path系列常数
		const static int BANK_SIZ_LIST_SIZE = 100;
		static double DIR_ASSESS_GO_SAFE(int ind, bool directed, const mix_score_t& scores);
		//scan_act系列常数
		const static int SCAN_ACT_MAX_DEPTH = 6;
		constexpr static double SCAN_ACT_NEAR_REDUCE[2] = {0.2,0.75};//敌,我
		constexpr static double SCAN_ACT_DIRE_REDUCE[2] = {0.6,0.9};//敌,我
		constexpr static double CRIT_AIR_PARAM[5] = {4.5,0.75,-8.0,-1.0,0.5};//(上限绝对值,上限比例,惩罚绝对值,惩罚leng系数,奖励score系数)
		constexpr static double LOW_AIR_PARAM[4] = {10,2.5,-0.8,0.2};//(上限绝对值,上限比例,惩罚系数,奖励score系数)
		//PA分系列常数
		// constexpr static double A_SMALL_BONUS[3] = {0,1.2,1.5};
		constexpr static double P_1_AIR_PARAM[2] = {-4,-0.4};
		constexpr static double P_NO_AIR_PARAM[2] = {-8,-1};
		constexpr static double A_AIR_MULT[2] = {0.7,0.3};//(0 air,1 air)
		constexpr static double A_ENV_BONUS[2] = {2,2};//(No laser, 4 snakes)
		//寻路系列常数
		static double GRED_ASSESS_REGULAR(int ind, bool directed, const mix_score_t& scores);
		static double DIR_ASSESS_REGULAR(int ind, bool directed, const mix_score_t& scores);
		constexpr static double EM_RAY_COST[2] = {2,1.0/3};
		constexpr static double EM_SOLID_EFF = 0.75;
		const static int EM_SOLID_COUNT_MIN = 3;
};

//Search类
const int SNK_SIMU_TYPE_MAX = 128;
//【snklst一定要包括snkid】【不允许在turn=513时调用search】
//【snkid必须是当前行动的蛇】
struct search_best {
	bool snk_dead;
	int actid;
	double val;
	inline bool isnull() const {
		if(this->actid == -1) assert(this->val == -1);
		return this->actid == -1;
	} 
};
const search_best SEARCH_BEST_NULL {false,-1,-1};
struct search_node {
	Context ctx;
	search_best best;
	int last_step;
	int fa,childs,returned;
	bool full_expended;
	bool max_layer;
};
class Search {
	public:
		Search(const Context& ctx0, Logger& logger, const vector<int>& snklst, int snkid);

		void setup_search(int max_turn, double (*value_func)(const Context& begin, const Context& end, int snkid), int act_maxv = 4);
		//发起局部搜索
		//返回的act_score_t中的actid【不是ACT的下标】
		void search();
		
		search_best results[ACT_MAXV];
		vector<search_node> node_list;
		queue<int> search_qu;
		
		bool snk_dead = false;

		int search_cnt = 0;
		int end_turn;
	private:
		Logger& logger;
		Context ctx0;
		const int camp;
		const int snkid;
		int act_maxv;
		int max_turn;
		int max_count = MAX_SEARCH_COUNT;
		bool snk_simu_type[SNK_SIMU_TYPE_MAX];
		bool stopping_snake[SNK_SIMU_TYPE_MAX];
		int not_simu_cnt = 0;
		double (*value_func)(const Context& begin, const Context& end, int snkid);

		void search_bfs();
		void update_value(const search_best& best, int fa, int src);

		
		const static int MAX_SEARCH_COUNT = 20000;
		const static int BASE_SEARCH_COUNT = 1000;
		const static int MAX_SEARCH_DEPTH = 7;
};
Search::Search(const Context& ctx0, Logger& logger, const vector<int>& snklst, int snkid) :
ctx0(ctx0), logger(logger), camp(ctx0.current_player()), snkid(snkid)
{
	this->node_list.clear();
	while(!this->search_qu.empty()) this->search_qu.pop();

	for(int i = 0; i < ACT_MAXV; i++) this->results[i] = SEARCH_BEST_NULL;
	for(int i = 0; i < SNK_SIMU_TYPE_MAX; i++) this->snk_simu_type[i] = true;//所有id都在模拟范围内（包括未来的id）
	for(int i = 0; i < SNK_SIMU_TYPE_MAX; i++) this->stopping_snake[i] = false;
	
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
		if(!keep) {
			snk_simu_type[*it] = false;//,printf("remove:%d\n",*it);//排除id
			not_simu_cnt++;
		}
	}
	// 帮ctx0跳
	while(!this->snk_simu_type[this->ctx0._current_snake_id]) this->ctx0.skip_operation();

	bool arrived = false;
	for(const Snake& snk : this->ctx0.my_snakes()) {
		if(snk.id == this->snkid) arrived = true;
		if(arrived) stopping_snake[snk.id] = true;
	}
	for(const Snake& snk : this->ctx0.opponents_snakes()) stopping_snake[snk.id] = true;
}
void Search::setup_search(int max_turn, double (*value_func)(const Context& begin, const Context& end, int snkid), int act_maxv) {
	this->act_maxv = act_maxv;
	this->max_turn = max_turn;
	this->value_func = value_func;
	this->end_turn = min(this->ctx0.current_round() + this->max_turn,this->ctx0.max_round());//【最后一回合可能搜不完全】
	printf("setup: start_turn:%d max_turn:%d end_turn:%d\n",this->ctx0.current_round(),this->max_turn,this->end_turn);
}
void Search::search() {
	if(this->ctx0.current_round() >= this->ctx0.max_round()) {
		for(int i = 0; i < ACT_MAXV; i++) this->results[i].val = 0;
		this->snk_dead = false;
		return;
	}

	this->search_bfs();
	for(int i = 1; i <= this->act_maxv && i < this->node_list.size(); i++) {
		if(this->node_list[i].fa == 0) {
			// this->logger.log(0,"collected action %d from %d (%.2f) dead:%d",this->node_list[i].last_step,i,this->node_list[i].best.val,this->node_list[i].best.snk_dead);
			this->results[this->node_list[i].last_step] = this->node_list[i].best;
		}
	}
	this->snk_dead = true;
	for(int i = 0; i < ACT_MAXV; i++) if(!this->results[i].isnull() && !this->results[i].snk_dead) this->snk_dead = false;
	if(this->snk_dead) this->logger.log(1,"搜索判定为必死状态");

	for(int i = 0; i < this->act_maxv; i++) if(this->results[i].isnull()) this->results[i].val = -100;

	this->logger.log(0,"searched:%d",this->search_cnt);
	if(this->search_cnt >= this->MAX_SEARCH_COUNT) this->logger.log(1,"局面数超出搜索限制");
}
void Search::search_bfs() {
	this->node_list.push_back(search_node({ctx0,SEARCH_BEST_NULL,-1,-1,0,0,false,true}));
	this->search_qu.push(0);

	while(!this->search_qu.empty()) {
		int now = this->search_qu.front();
		this->search_qu.pop();
		this->logger.flush();

		assert(this->node_list[now].ctx.current_round() <= this->end_turn + 1);
		// if(this->node_list[now].ctx.current_round() == this->end_turn) this->logger.log(0,"now at : [%d],fa:%d lst:%d curr_snk:%d snk_camp:%d curr_round:%d",now,this->node_list[now].fa,this->node_list[now].last_step,this->node_list[now].ctx._current_snake_id,this->node_list[now].ctx.current_snake().camp,this->node_list[now].ctx.current_round());

		//检查终止条件
		bool end = false;
		bool time_up = (this->node_list[now].ctx.current_round() == this->end_turn && this->stopping_snake[this->node_list[now].ctx._current_snake_id]) || this->node_list[now].ctx.current_round() > this->end_turn;
		if(time_up) {//到时间
			if(end_turn >= this->ctx0.current_round()+this->MAX_SEARCH_DEPTH || end_turn >= this->ctx0.max_round()) end = true;
			else if(this->search_cnt >= BASE_SEARCH_COUNT) end = true;
			else {
				this->max_count = BASE_SEARCH_COUNT;
				end_turn++;
				max_turn++;
				this->logger.log(1,"搜索加深至%d回合",max_turn);
			}
			// end = true;
		}
		if(this->node_list[now].ctx.my_snakes().size() + this->node_list[now].ctx.opponents_snakes().size() == not_simu_cnt) end = true;//蛇全死
		if(this->search_cnt >= this->max_count) end = true;//局面超限
		//剪枝(尚未确认)
		int fa = this->node_list[now].fa;
		if(!this->node_list[now].best.isnull() && !this->node_list[fa].best.isnull()) {//根节点会被第一个条件筛掉
			if(this->node_list[now].max_layer && !this->node_list[fa].max_layer && this->node_list[now].best.val >= this->node_list[fa].best.val) end = true;
			else if (!this->node_list[now].max_layer && this->node_list[fa].max_layer && this->node_list[now].best.val <= this->node_list[fa].best.val) end = true;
		}
		if(end) {//不再扩展
			const double score = (*this->value_func)(this->ctx0,this->node_list[now].ctx,this->snkid);
			this->node_list[now].best = search_best({!this->node_list[now].ctx.inlist(this->snkid),this->node_list[now].last_step,score});
			this->update_value(this->node_list[now].best,this->node_list[now].fa,now);
			continue;
		}

		//开始扩展
		bool crushed = false;
		bool must_die = this->node_list[now].ctx.must_die();
		for(int i = 0; i < this->act_maxv; i++) {
			if(!this->node_list[now].ctx.check_operation(i+1)) continue;
			if(!must_die && i < ACT_LEN && !this->node_list[now].ctx.not_crushing(i)) continue;
			if(must_die && i < ACT_LEN) {
				const Coord next = this->node_list[now].ctx.current_snake()[0] + ACT_CRD[i];
				if(this->node_list[now].ctx.snake_map()[next.x][next.y] != this->node_list[now].ctx._current_snake_id) {
					if(!crushed) crushed = true;
					else continue;
				}
			}

			//先初始化ctx
			// this->logger.log(0,"push:[%d]->[%d] last:%d",now,this->node_list.size(),i);
			this->node_list.push_back(search_node({this->node_list[now].ctx,SEARCH_BEST_NULL,i,now,0,0,false,false}));
			this->node_list[now].childs++;
			this->node_list.back().ctx.do_operation(Operation({i+1}));
			
			//如果全撞死了,直接返回
			if(this->node_list.back().ctx.my_snakes().size() + this->node_list.back().ctx.opponents_snakes().size() <= not_simu_cnt) {//但为啥会小于呢...
				const double score = (*this->value_func)(this->ctx0,this->node_list.back().ctx,this->snkid);
				this->node_list.back().best = search_best({true,i,score});
				this->update_value(this->node_list.back().best,now,this->node_list.size()-1);
				continue;
			}

			while(!this->snk_simu_type[this->node_list.back().ctx._current_snake_id]) this->node_list.back().ctx.skip_operation();
			if(this->node_list.back().ctx.current_snake().camp == this->camp) this->node_list.back().max_layer = true;


			this->search_cnt++;
			this->search_qu.push(this->node_list.size()-1);
		}
		this->node_list[now].full_expended = true;
		if(this->node_list[now].returned == this->node_list[now].childs && this->node_list[now].fa != -1)
			this->update_value(search_best({this->node_list[now].ctx.inlist(this->snkid),this->node_list[now].last_step,this->node_list[now].best.val}),this->node_list[now].fa,now);
	}
}
void Search::update_value(const search_best& best, int fa, int src) {
	assert(!best.isnull());

	search_node& tgt = this->node_list[fa];
	// if(src <= 1000) this->logger.log(0,"return [%d]_%d->[%d] max:%d (%.2f) fa_curr:%.2f",src,best.actid,fa,tgt.ctx.current_snake().camp == this->camp,best.val,this->node_list[fa].best.val);
	// this->logger.flush();

	if(tgt.best.isnull()) tgt.best = best;
	else if(tgt.max_layer && best.val > tgt.best.val) tgt.best = best;
	else if(!tgt.max_layer && best.val < tgt.best.val) tgt.best = best;

	tgt.returned++;
	if(tgt.returned == tgt.childs && tgt.full_expended && tgt.fa != -1) this->update_value(search_best({tgt.best.snk_dead,tgt.last_step,tgt.best.val}), tgt.fa, fa);
}

//Ai类
bool vis[MAP_LENGTH][MAP_LENGTH];
AI::AI(Logger &logger) : 
logger(logger)
{
	
}
int AI::judge(const Snake &snake, const Context &ctx) {
	this->snake = &snake;
	this->ctx = &ctx;
	this->turn = ctx.current_round();
	this->logger.config(this->turn,snake.id);
	this->logger.log(1,"");

	Assess &&assess_instance = Assess(*this,ctx,this->logger,snake.id);
	this->assess = &assess_instance;


	if(this->last_turn != ctx.current_round()) {
		this->last_turn = ctx.current_round();
		if(ctx.current_round() == 1) this->total_init();
		this->turn_control();
	}

	//测试区

	int tgt_type = this->target_list[this->snake->id].type;
	if(tgt_type == 4) return this->do_dead_signal()+1;
	if(tgt_type == 5) return this->do_command() + 1;

	if(this->try_shoot()) return 5;

	const int sol = this->try_solid();
	if(sol != -1) return sol+1;

	if(tgt_type == 0) return this->do_eat()+1;
	if(tgt_type == 1) return this->do_kl()+1;
	if(tgt_type == 2) return this->do_shoot()+1;
	if(tgt_type == 3) return this->do_solid()+1;

	if(tgt_type == -1) {
		this->logger.log(1,"未分配到目标");

		if(this->try_split()) {
			this->logger.log(1,"主动分裂，长度%d",this->snake->length());
			return 6;
		}
		return this->assess->go_safe() + 1;
	}

	assert(false);
}
void AI::total_init() {
	this->target_list.clear();//【不能理解】
	this->solid_addons.clear();
	for(int i = 0; i < MAP_LENGTH; i++) this->x_wall_alloc[i] = this->y_wall_alloc[i] = -1;
	for(int i = 0; i < ALLOC_MAX; i++) this->dead_signal_alloc[i] = -1;
}
void AI::turn_control() {
	this->assess->do_turn_assess();

	this->turn_init();

	// this->calc_safe_area();
	this->plan_kl();
	this->plan_shoot();
	this->plan_solid();

	this->plan_dead_signal();
	this->distribute_tgt();
}
void AI::turn_init() {
	for(const Snake& snk : this->ctx->my_snakes()) if(!this->target_list.count(snk.id)) this->target_list[snk.id] = NULL_TARGET;//别搞得没target
}
void AI::calc_safe_area() {
	for(int i = 0; i < 4; i++) this->area_alloc[i] = this->area_occupy_snk[i] = this->area_occupy_val[i] = 0;
	for(int x = 0; x < 16; x++) for(int y = 0; y < 16; y++) if(this->ctx->wall_map()[x][y] != -1) this->area_occupy_val[this->__get_area(x,y)]++;
	for(int x = 0; x < 16; x++) for(int y = 0; y < 16; y++) if(this->ctx->snake_map()[x][y] != -1) this->area_occupy_val[this->__get_area(x,y)]++;
	for(const Snake& snk : this->ctx->my_snakes()) {
		this->safe_area_alloc[snk.id] = NULL_COORD;
		this->area_occupy_snk[this->__get_area(snk[0].x,snk[0].y)]++;
		this->area_occupy_val[this->__get_area(snk[0].x,snk[0].y)] += 4;
	}
	for(const Snake& snk : this->ctx->opponents_snakes()) this->area_occupy_val[this->__get_area(snk[0].x,snk[0].y)] += 4;

	int ta = 0, tb = 0, tc = 0;
	for(int i = 0; i < 4; i++) {
		if(this->area_occupy_val[i] <= 16) this->area_level[i] = 1,ta++;
		else if(this->area_occupy_val[i] <= 32) this->area_level[i] = 2,tb++;
		else this->area_level[i] = 3,tc++;
	}
	this->logger.log(1,"安全区扫描:%d空旷,%d一般,%d拥挤",ta,tb,tc);

	for(const Snake& snk : this->ctx->my_snakes()) {//拥挤区内蛇疏散
		if(snk.length() <= this->SAREA_SHORT_LENG) continue;
		if(this->__get_area(snk[0].x,snk[0].y) != 3) continue;

		pii best = {-1,100};//(id,dis)
		for(int i = 0; i < 4; i++) {
			const Coord& mid = this->__get_mid(i);
			if(this->area_level[i] == 3) continue;
			int dis = this->__get_dist(i,snk.id);

			if(dis != -1 && dis < best.second) best = {i,dis};
		}

		if(best.first != -1) {
			this->area_alloc[best.first] = true;
			this->safe_area_alloc[snk.id] = this->__get_mid(best.first);
			this->logger.log(1,"安全区分配(疏散):蛇%2d->区域%d 中心%s",snk.id,best.first,this->__get_mid(best.first).to_string().c_str());
		}
	}
	for(int i = 0; i < 4; i++) {//空旷分区
		if(this->area_level[i] != 1 || this->area_occupy_snk[i] || this->area_alloc[i]) continue;
		for(int j = 0; j < 4; j++) {//有2蛇的分区
			if(this->area_occupy_snk[j] < 2 || i == j) continue;

			pii best = {-1,100};//(id,dis)
			for(const Snake& snk : this->ctx->my_snakes()) {//其中的蛇
				int pos = this->__get_area(snk[0].x,snk[0].y);
				if(pos != j || this->safe_area_alloc[snk.id] != NULL_COORD) continue;
				
				int dis = this->__get_dist(i,snk.id);
				if(dis != -1 && dis < best.second) best = {i,dis};
			}
			if(best.first != -1) {
				this->area_alloc[i] = true;
				this->safe_area_alloc[best.first] = this->__get_mid(i);
				this->logger.log(1,"安全区分配(均衡):蛇%2d->区域%d 中心%s",best.first,i,this->__get_mid(i).to_string().c_str());
			}
		}
	}
}
inline int AI::__get_dist(int aid, int snkid) {
	const Coord&& mid = this->__get_mid(aid);
	int tmp1 = this->assess->get_adjcent_dis(mid.x,mid.y,snkid);
	int tmp2 = this->assess->get_adjcent_dis(mid.x+1,mid.y+1,snkid);
	int tmp3 = this->assess->get_adjcent_dis(mid.x-1,mid.y-1,snkid);
	if(tmp1 == -1) tmp1 = 100;
	if(tmp2 == -1) tmp2 = 100;
	if(tmp3 == -1) tmp3 = 100;
	int ans = min(tmp1,min(tmp2,tmp3));
	return (ans == 100) ? -1 : ans;
}
void AI::plan_kl() {
	//自动解除一些进攻目标
	for(const Snake& snk : this->ctx->my_snakes()) {
		const target& tgt = this->target_list[snk.id];
		if(tgt.type != 1) continue;

		const target_kl& kl_addon = this->kl_addons[snk.id];
		const Snake& attacker = this->ctx->find_snake(kl_addon.killer_id);
		if(this->turn - tgt.time > this->KL_TIME_LIM && kl_addon.status == 0) {
			this->logger.log(1,"进攻超时");
			this->release_target(snk.id);
		} else if(!this->ctx->inlist(kl_addon.tgt_id)) {
			this->logger.log(1,"目标死亡");
			this->release_target(snk.id);
		} else if(this->ctx->find_snake(kl_addon.tgt_id).length() < KL_TGT_ABA_MIN_LENG) {
			this->logger.log(1,"目标过短");
			this->release_target(snk.id);
		} else {
			const Coord tgt_pos = this->ctx->find_snake(kl_addon.tgt_id)[0];
			const int dis = this->assess->get_adjcent_dis(tgt_pos.x,tgt_pos.y,attacker.id);
			if((dis > this->KL_MAX_NORM_DIS && kl_addon.type == 0) || dis > this->KL_MAX_HELP_DIS) {
				this->logger.log(1,"距离超限");
				this->release_target(snk.id);
			}
		}
	}

	//开始分析新的kl
	this->kl_list.clear();
	for(const Snake& enemy : this->ctx->opponents_snakes()) {
		if(enemy.length() < this->KL_TGT_MIN_LENG) continue;//从4格长开始
		// if(!this->assess->check_go_straight(enemy.id,3)) continue;//3格内走直线
		
		for(const Snake& ally : this->ctx->my_snakes()) {
			if(this->target_list[ally.id].type == 1 || ally.length() > this->KL_MAX_ALLY_LENG) continue;//不在追人，长度达标

			const int dist = this->assess->get_adjcent_dis(enemy[0].x,enemy[0].y,ally.id);
			if(dist > this->KL_MAX_NORM_DIS || dist == -1) continue;//2格内贴头
			
			const double enc_angle = this->assess->get_encounter_angle(ally.id,enemy.id);
			if((enc_angle > 155 || enc_angle < 85) && dist >= 2) continue;//接近角正确

			// this->logger.log(0,"初筛通过 %d->%d angle:%.1f",ally.id,enemy.id,enc_angle);

			//至此已经确认可以try一次kill或limit了
			//以下考察有没有静态的wall

			int wall_leng = -1, turn_dist = 0;
			const Coord&& tgt_dire = enemy[0]-enemy[1];
			Coord check_pos = NULL_COORD;
			if(tgt_dire.x == 0) {
				if(abs(ally[0].x-(enemy[0].x+1)) > abs(ally[0].x-(enemy[0].x-1))) check_pos = Coord({enemy[0].x+1,enemy[0].y});
				else check_pos = Coord({enemy[0].x-1,enemy[0].y});
			} else {
				if(abs(ally[0].y-(enemy[0].y+1)) > abs(ally[0].y-(enemy[0].y-1))) check_pos = Coord({enemy[0].x,enemy[0].y+1});
				else check_pos = Coord({enemy[0].x,enemy[0].y-1});
			}
			check_pos = check_pos - tgt_dire;

			while(true) {
				wall_leng++;
				check_pos = check_pos + tgt_dire;
				if(check_pos.x < 0 || check_pos.y < 0 || check_pos.x >= this->ctx->length() || check_pos.y >= this->ctx->width()) break;
				if(this->ctx->wall_map()[check_pos.x][check_pos.y] != -1) continue;
				if(this->ctx->snake_map()[check_pos.x][check_pos.y] != -1) {
					const int block_snk = this->ctx->snake_map()[check_pos.x][check_pos.y];
					if(!this->assess->check_nstep_norm(check_pos.x,check_pos.y,wall_leng-1,enemy.id)) continue;
				}
				break;
			}

			//以下考察有没有现成的turn
			Coord turn_pos = enemy[0];
			while(true) {
				turn_dist++;
				turn_pos = turn_pos + tgt_dire;
				if(turn_pos.x < 0 || turn_pos.y < 0 || turn_pos.x >= this->ctx->length() || turn_pos.y >= this->ctx->width()) break;
				if(this->ctx->wall_map()[turn_pos.x][turn_pos.y] != -1) break;
				if(this->ctx->snake_map()[turn_pos.x][turn_pos.y] != -1) {
					const int block_snk = this->ctx->snake_map()[turn_pos.x][turn_pos.y];
					if(!this->assess->check_nstep_norm(check_pos.x,check_pos.y,turn_dist-1,enemy.id)) continue;
				}
			}

			if(wall_leng == 0) {//limit-2
				this->kl_list.push_back(kl_alloc_t({enemy.id,ally.id,3,int(dist<=1),-9,-9,0,turn_dist}));
				this->logger.log(0,"潜在limit-2 %d->%d sure:%d",ally.id,enemy.id,int(dist<=1));
				continue;
			}
			if(turn_dist == -1 || turn_dist > wall_leng) {//无现成turn，limit-1
				this->kl_list.push_back(kl_alloc_t({enemy.id,ally.id,2,int(dist<=1),-1,-9,wall_leng,turn_dist}));
				this->logger.log(0,"潜在limit-1 %d->%d sure:%d",ally.id,enemy.id,int(dist<=1));
				continue;
			}
			if(turn_dist <= wall_leng) {//kill-1
				this->kl_list.push_back(kl_alloc_t({enemy.id,ally.id,0,int(dist<=1),-1,-1,wall_leng,turn_dist}));
				this->logger.log(0,"潜在kill-1 %d->%d sure:%d wall:%d turn:%d",ally.id,enemy.id,int(dist<=1),wall_leng,turn_dist);
				continue;
			}
		}
	}

	//开始分析支援目标
	for(const Snake& attacker : this->ctx->my_snakes()) {
		if(this->target_list[attacker.id].type != 1 || this->kl_addons[attacker.id].type != 0) continue;

		const target_kl& kl_addon = this->kl_addons[attacker.id];
		const Snake& enemy = this->ctx->find_snake(kl_addon.tgt_id);
		for(const Snake& ally : this->ctx->my_snakes()) {
			if(ally.id == attacker.id) continue;

			const int dist = this->assess->get_adjcent_dis(enemy[0].x,enemy[0].y,ally.id);
			if(dist > this->KL_MAX_HELP_DIS || dist == -1) continue;//太远
			if(dist < this->assess->get_adjcent_dis(enemy[0].x,enemy[0].y,attacker.id)) continue;//反侧
			
			const double enc_angle = (enemy[0]-attacker[0]).get_angle(enemy[0]-ally[0]);
			if(enc_angle < 90) continue;//接近角正确

			this->logger.log(0,"潜在援助 %d->%d sure:%d",ally.id,enemy.id,int(dist<=1));
			this->kl_list.push_back(kl_alloc_t({enemy.id,ally.id,4,int(dist<=1),-1,-9,-1,-1}));
		}
	}
}
void AI::plan_shoot() {
	//解除一些目标
	for(const Snake& snk : this->ctx->my_snakes()) {
		const target& tgt = this->target_list[snk.id];
		if(tgt.type != 2) continue;

		if(this->turn - tgt.time > 12) {
			this->logger.log(1,"融墙超时");
			this->release_target(snk.id);
			continue;
		}
		if(!this->assess->can_shoot(snk.id)) {
			this->logger.log(1,"武器丢失");
			this->release_target(snk.id);
			continue;
		}

		shoot_alloc_t& addon = this->shoot_addons[snk.id];
		const pii&& result = this->assess->ray_trace_dir(addon.pos,ACT_CRD[addon.turn_dir]);
		addon.gain = result.second, addon.loss = result.first;
		if(result.second - result.first <= 3) {
			this->logger.log(1,"角度不再合适");
			this->release_target(snk.id);
			continue;
		}

		const int dist = this->assess->get_adjcent_dis(addon.pos.x,addon.pos.y,snk.id);
		if(dist > 5 || dist == -1) {
			this->logger.log(1,"距离超限");
			this->release_target(snk.id);
			continue;
		}
	}

	//开始计算
	this->shoot_list.clear();
	for(int x = 0; x < MAP_LENGTH; x++) {
		for(int y = 0; y < MAP_LENGTH; y++) {
			if(this->ctx->wall_map()[x][y] != -1) continue;//无墙，但蛇算障碍吗？

			const Coord reg_pos = Coord({x,y});
			for(int turn = 0; turn < ACT_LEN; turn++) {
				int tx = x + ACT[turn][0];
				int ty = y + ACT[turn][1];
				if(tx < 0 || ty < 0 || tx >= MAP_LENGTH || ty >= MAP_LENGTH) continue;
				if(this->ctx->wall_map()[tx][ty] != -1) continue;

				const pii&& result = this->assess->ray_trace_dir(Coord({tx,ty}),ACT_CRD[turn]);
				if(result.second - result.first < 5) continue;

				this->shoot_list.push_back(shoot_alloc_t({result.second,result.first,reg_pos,turn}));
			}
		}
	}
}
void AI::plan_solid() {
	//解除一些目标
	for(const Snake& snk : this->ctx->my_snakes()) {
		const target& tgt = this->target_list[snk.id];
		if(tgt.type != 3) continue;

	}

	//开始计算
	this->solid_list.clear();
	for(const Snake& snk : this->ctx->my_snakes()) {
		int leng = snk.length() + snk.length_bank;
		if(snk.length() == 1 || leng < 10) continue;

		// int nearest = 100;
		// for(const Snake& _enemy : this->ctx->opponents_snakes()) {
		// 	int dis = this->assess->get_adjcent_dis(_enemy[0].x,_enemy[0].y,snk.id);
		// 	if(dis != 0 && dis < nearest) nearest = dis;
		// }
		// if(nearest+1 <= 5) continue;

		int long_side = (leng+5) / 4;
		int short_side = (leng+3) / 4;
		const Coord&& dire = snk[0] - snk[1];
		const Coord&& verti = dire.get_verti();
		const Coord dirs[2] = {dire,verti};
		
		for(int di = 0; di <= 1; di++) {
			if(di == 1 && long_side == short_side) break;
			for(int sig = 1; sig != -3; sig -= 2) {
				vector<Coord> pos_list1 = {snk[0]+dirs[di]*sig*(long_side-1),
										snk[0]+dirs[di]*sig*(long_side-1)+dirs[1-di]*(short_side-1),
										snk[0]+dirs[1-di]*(short_side-1),
										snk[0]};
				// printf("%s->%s->%s->%s\n",snk[0].to_string().c_str(),pos_list1[0].to_string().c_str(),pos_list1[1].to_string().c_str(),pos_list1[2].to_string().c_str());
				this->_check_solid_push(target_solid({snk.id,long_side,short_side,-1,-1,-1,-1,snk[0],pos_list1,vector<int>(),false}));
				vector<Coord> pos_list2 = {snk[0]+dirs[di]*sig*(long_side-1),
										snk[0]+dirs[di]*sig*(long_side-1)-dirs[1-di]*(short_side-1),
										snk[0]-dirs[1-di]*(short_side-1),
										snk[0]};
				this->_check_solid_push(target_solid({snk.id,long_side,short_side,-1,-1,-1,-1,snk[0],pos_list2,vector<int>(),false}));
			}
		}

		if(long_side > short_side) long_side--;
		else short_side--;
		for(int di = 0; di <= 1; di++) {
			if(di == 1 && long_side == short_side) break;
			for(int sig = 1; sig != -3; sig -= 2) {
				vector<Coord> pos_list1 = {snk[0]+dirs[di]*sig*(long_side-1),
										snk[0]+dirs[di]*sig*(long_side-1)+dirs[1-di]*(short_side-1),
										snk[0]+dirs[1-di]*(short_side-1),
										snk[0]};
				// printf("%s->%s->%s->%s\n",snk[0].to_string().c_str(),pos_list1[0].to_string().c_str(),pos_list1[1].to_string().c_str(),pos_list1[2].to_string().c_str());
				this->_check_solid_push(target_solid({snk.id,long_side,short_side,-1,-1,-1,-1,snk[0],pos_list1,vector<int>(),false}));
				vector<Coord> pos_list2 = {snk[0]+dirs[di]*sig*(long_side-1),
										snk[0]+dirs[di]*sig*(long_side-1)-dirs[1-di]*(short_side-1),
										snk[0]-dirs[1-di]*(short_side-1),
										snk[0]};
				this->_check_solid_push(target_solid({snk.id,long_side,short_side,-1,-1,-1,-1,snk[0],pos_list2,vector<int>(),false}));
			}
		}
	}
}
void AI::_check_solid_push(target_solid&& addon) {
	for(const Coord& next : addon.pos_list) if(next.x < 0 || next.y < 0 || next.x >= MAP_LENGTH || next.y >= MAP_LENGTH) return;//坐标初步验证
	addon.cover_size = addon.short_side * addon.long_side;//可以考虑把敌人的墙加进去
	addon.require_length = (addon.short_side + addon.long_side) * 2 + 1;

	int tot_dis = 0;
	Coord now = addon.origin;
	for(int i = 0; i < addon.pos_list.size(); i++) {
		const Coord seg_tgt = addon.pos_list[i];
		int seg_dis = now.get_block_dist(seg_tgt);
		tot_dis += seg_dis;

		addon.dist_list.push_back(tot_dis);

		// printf("loop: %s->%s\n",now.to_string().c_str(),seg_tgt.to_string().c_str());
		int seg_length = 0;
		const Coord dire = Coord({(seg_tgt.x-now.x)/seg_dis,(seg_tgt.y-now.y)/seg_dis});
		for(Coord _check = now; _check != seg_tgt+dire; _check = _check + dire, seg_length++) {//保证区段畅通
			if(this->ctx->wall_map()[_check.x][_check.y] != -1) return;
			if(_check == addon.origin) continue;//特判头部

			int block_snk = this->ctx->snake_map()[_check.x][_check.y];
			if(block_snk != -1) {
				if(block_snk != addon.snkid) return;//包括队友

				int leave_time = this->assess->get_pos_on_snake(_check) + this->ctx->find_snake(addon.snkid).length_bank;//处理自己身子的问题
				if(tot_dis + seg_length < leave_time) return;
			}
		}

		const spd_map_t& e_dist = this->assess->enemy_spd[now.x][now.y];
		if(e_dist != SPD_MAP_NULL && e_dist.dist < tot_dis-1) return;//不能有敌人更快(快多1格)
		now = seg_tgt;
	}
	addon.tot_dist = tot_dis;

	int extra = 0;
	int xmin = min(addon.pos_list[0].x,addon.pos_list[2].x), xmax = max(addon.pos_list[0].x,addon.pos_list[2].x);
	int ymin = min(addon.pos_list[0].y,addon.pos_list[2].y), ymax = max(addon.pos_list[0].y,addon.pos_list[2].y);
	for(int x = xmin; x <= xmax; x++) {
		for(int y = ymin; y <= ymax; y++) {
			int wall = this->ctx->wall_map()[x][y];
			if(wall == this->ctx->current_player()) extra--;
			else if(wall != -1) extra++;
		}
	}
	addon.remove_wall = extra;

	assert(addon.pos_list.size() >= 3);
	this->logger.log(0,"check passed for snake %d: size%d(rm%d) ->%s->%s->%s",addon.snkid,addon.cover_size,addon.remove_wall,addon.pos_list[0].to_string().c_str(),addon.pos_list[1].to_string().c_str(),addon.pos_list[2].to_string().c_str());
	this->solid_list.push_back(addon);
}
void AI::plan_dead_signal() {
	for(const Snake& snk : this->ctx->my_snakes()) {
		if(this->target_list[snk.id].type != 4) continue;
		const dead_signal& sig = this->dead_signal_addons[snk.id];
		if(!this->ctx->inlist(sig.snkid)) {
			this->logger.log(1,"救援目标消失");
			this->release_target(snk.id);
			continue;
		}
		pii&& best = this->assess->get_enclosing_leng(sig.snkid);
		if(best.first >= 0.66*(sig.leng+sig.bank) || best.first >= int(this->snake->length())-3) {
			this->logger.log(1,"不再符合救援标准");
			this->release_target(snk.id);
			continue;
		}
	}

	for(const Snake& snk : this->ctx->my_snakes()) {
		if(this->assess->find_dead(snk.id) == -1 || this->target_list[snk.id].type == 4) continue;
		
		//考虑牺牲者cost
		double cost = snk.length() + snk.length_bank/2.0;
		const pii&& best = this->assess->get_enclosing_leng(snk.id);//【area不支持snkid就离谱】
		if(this->target_list[snk.id].type == 0) cost += 1;
		if(this->target_list[snk.id].type == 1 || this->target_list[snk.id].type == 2) cost += 3;
		if(this->target_list[snk.id].type == 3) cost -= 2;

		if(best.second != -1) cost -= 0.8*best.first;


		for(const dead_signal& sig : this->dead_signal_list) {
			if(!this->ctx->inlist(sig.snkid)) continue;
			if(this->dead_signal_alloc[sig.snkid] != -1 || snk.id == sig.snkid) continue;//有人去救了

			double final_cost = cost;
			//考虑被救者cost
			final_cost -= sig.leng/2.0 + sig.bank/3.0;
			if(sig.laser) final_cost -= 2;
		
			if(final_cost < 0) {//确认救援
				this->logger.log(1,"任务分配(牺牲) 蛇%2d救援%2d 代价%.2f",snk.id,sig.snkid,final_cost);
				this->release_target(snk.id);
				this->target_list[snk.id] = target({4,this->turn});
				this->dead_signal_alloc[sig.snkid] = snk.id;
				this->dead_signal_addons[snk.id] = sig;
			}
		}
	}
	this->dead_signal_list.clear();
}
void AI::distribute_tgt() {
	//清除食物类目标
	for(int i = 0; i < ALLOC_MAX; i++) this->item_alloc[i] = -1;
	for(auto it = this->ctx->my_snakes().begin(); it != this->ctx->my_snakes().end(); it++) if(this->target_list[it->id].type == 0) this->release_target(it->id,true);

	//help类目标优先
	for(const kl_alloc_t& kl : this->kl_list) {
		const int atk_id = kl.killer_id;
		const int tgt_id = kl.tgt_id;
		const Snake& attacker = this->ctx->find_snake(atk_id);

		if(!this->target_list[atk_id].isnull() || kl.type != 4) continue;

		this->logger.log(1,"目标分配(进攻+支援):蛇%2d->%2d",kl.killer_id,kl.tgt_id);
		this->kl_addons[atk_id] = target_kl({kl.tgt_id,kl.killer_id,kl.status,1,kl.waller_id,kl.turner_id});
		this->target_list[atk_id] = target({1,this->turn});
	}

	//分配食物类目标(第一轮)
	vector<item_alloc_t> item_list;
	for(auto _item = this->ctx->item_list().begin(); _item != this->ctx->item_list().end(); _item++) {
		if(_item->eaten || _item->expired || this->assess->check_item_captured_team(*_item) != -1) continue;//排除已被吃/将被吃
		if(_item->time - this->turn > this->ITEM_FTR_LIMIT) continue;//太过久远

		for(auto _friend = this->ctx->my_snakes().begin(); _friend != this->ctx->my_snakes().end(); _friend++) {
			if(this->target_list[_friend->id].type != -1) continue;//如果已有目标，则跳过
			const int dst = (*this->assess->dist_map[_friend->id])[_item->x][_item->y];
			if(dst == -1 || this->turn+dst >= _item->time+ITEM_EXPIRE_LIMIT) continue;

			const spd_map_t &fastest = this->assess->tot_spd[_item->x][_item->y];
			if(dst - fastest.dist > this->ITEM_COMPETE_LIMIT) continue;//抢不过就不抢

			const int snkid = _friend->id;
			double cost = max(dst,_item->time - this->turn);//max(空间,时间)
			if(fastest.snkid != snkid) cost += this->ITEM_SLOW_COST_PENA * (dst-fastest.dist);
			if(_item->type == 0) cost -= this->ITEM_APPLE_SIZ_GAIN * _item->param;
			else {
				cost -= this->ITEM_APPLE_SIZ_GAIN * this->ITEM_LASER_AS_APPLE;
				cost += this->ITEM_HAS_LASER_PENA * int(this->assess->has_laser(snkid));
			}

			item_list.push_back(item_alloc_t({snkid,cost,*_item}));
		}
	}
	sort(item_list.begin(),item_list.end(),item_alloc_cmp);
	int complete_cnt = 0;
	for(auto it = item_list.begin(); it != item_list.end(); it++) {
		if(complete_cnt >= this->ctx->my_snakes().size()) break;
		if(this->item_alloc[it->item.id] != -1 || this->target_list[it->snkid].type != -1) continue;
		if(it->cost >= this->ITEM_MAX_COST_BOUND) continue;

		bool neared = false;
		for(const Snake& snk : this->ctx->my_snakes()) {
			if(this->target_list[snk.id].type != 0) continue;
			const Item& ori_tgt = this->ctx->find_item(this->item_addons[snk.id]);
			if(Coord({ori_tgt.x,ori_tgt.y}).get_block_dist(Coord({it->item.x,it->item.y})) <= 2) {
				neared = true;
				this->logger.log(0,"阻止临近分配:新目标%s",it->item.to_string().c_str());//难道Item的to_string不能两个连着用吗？
				break;
			}
		}
		if(neared) continue;

		this->logger.log(1,"目标分配(item):蛇%2d -> %s 代价%.1f",it->snkid,it->item.to_string().c_str(),it->cost);
		this->item_alloc[it->item.id] = it->snkid;
		this->item_addons[it->snkid] = it->item.id;
		this->target_list[it->snkid] = target({0,this->turn});
	}

	//进攻区段
	int attack_cnt = 0;
	for(const Snake& snk : this->ctx->my_snakes()) if(this->target_list[snk.id].type == 1 || this->target_list[snk.id].type == 2) attack_cnt++;
	for(const kl_alloc_t& kl : this->kl_list) {
		const int atk_id = kl.killer_id;
		const int tgt_id = kl.tgt_id;
		const Snake& attacker = this->ctx->find_snake(atk_id);

		if(!this->target_list[atk_id].isnull()) continue;
		if(attack_cnt >= 2) break;

		this->logger.log(1,"目标分配(进攻):蛇%2d->%2d",kl.killer_id,kl.tgt_id);
		this->kl_addons[atk_id] = target_kl({kl.tgt_id,kl.killer_id,kl.status,0,kl.waller_id,kl.turner_id});
		this->target_list[atk_id] = target({1,this->turn});
	}

	//融化区段
	for(const Snake& snk : this->ctx->my_snakes()) {
		if(!this->target_list[snk.id].isnull()) continue;
		if(!this->assess->can_shoot(snk.id)) continue;

		double best_v = -1000;
		shoot_alloc_t best_alloc;//效率有限,但算了吧...
		for(const shoot_alloc_t& alloc : this->shoot_list) {
			if((alloc.turn_dir % 2) && this->x_wall_alloc[alloc.pos.x] != -1) continue;//x方向（向y方向拐）
			if(alloc.turn_dir % 2 == 0 && this->y_wall_alloc[alloc.pos.y] != -1) continue;

			int dist = this->assess->get_adjcent_dis(alloc.pos.x,alloc.pos.y,snk.id);
			if(dist > 5 || dist == -1) continue;

			double score = alloc.gain - alloc.loss;
			score -= dist * 2;
			if(best_v < score) {
				best_v = score;
				best_alloc = alloc;
			}
		}
		if(best_v <= -900) continue;

		this->logger.log(1,"目标分配(融墙):蛇%2d->%s,净赚%d格",snk.id,best_alloc.pos.to_string().c_str(),best_alloc.gain-best_alloc.loss);
		this->shoot_addons[snk.id] = best_alloc;
		if(best_alloc.turn_dir % 2) this->x_wall_alloc[best_alloc.pos.x] = snk.id;
		else this->y_wall_alloc[best_alloc.pos.y] = snk.id;

		this->target_list[snk.id] = target({2,this->turn});
	}

	// 固化区段
	sort(this->solid_list.begin(),this->solid_list.end());
	int solid_cnt = 0;
	int tot_length[2] = {0,0};
	for(const Snake& snk : this->ctx->my_snakes()) {
		tot_length[0] += snk.length() + snk.length_bank/2;
		if(this->target_list[snk.id].type == 3) solid_cnt++;
	}
	for(const Snake& snk : this->ctx->opponents_snakes()) tot_length[1] += snk.length() + snk.length_bank/2;
	for(const target_solid& sol : this->solid_list) {
		const Snake& snk = this->ctx->find_snake(sol.snkid);
		if(!this->target_list[snk.id].isnull()) continue;
		if(solid_cnt >= 2 || this->ctx->my_snakes().size() <= 3 || (tot_length[0] < 35 && tot_length[0] < tot_length[1]+5)) break;
		
		this->logger.log(1,"目标分配(solid):蛇%2d",snk.id);
		this->target_list[snk.id] = target({3,this->turn});
		this->solid_addons[snk.id] = sol;
		this->logger.log(0,"solid目标坐标: ->%s->%s->%s",sol.pos_list[0].to_string().c_str(),sol.pos_list[1].to_string().c_str(),sol.pos_list[2].to_string().c_str());
		solid_cnt++;
	}
}
bool AI::try_split() {
	const Coord& tail = this->snake->coord_list.back();
	if(!this->assess->can_split() || this->assess->calc_snk_air(tail) < 2 || this->ctx->my_snakes().size() + this->reserved_size == 4) return false;

	//跑一个对尾部的简单空间统计
	int space = 0;
	queue<scan_act_q_t> qu;
	qu.push(scan_act_q_t({tail.x,tail.y,0,1}));
	for(int i = 0; i < MAP_LENGTH; i++) for(int j = 0; j < MAP_LENGTH; j++) vis[i][j] = false;
	vis[tail.x][tail.y] = true;
	while(!qu.empty()) {
		const scan_act_q_t now = qu.front();
		qu.pop();
		if(now.step > 6) continue;

		for(int i = 0; i < ACT_LEN; i++) {
			const int tx = now.x+ACT[i][0],ty = now.y+ACT[i][1];
			if(this->ctx->snake_map()[tx][ty] != -1 || this->ctx->wall_map()[tx][ty] != -1 || vis[tx][ty]) continue;
			vis[tx][ty] = 1;
			space++;
			qu.push(scan_act_q_t({tx,ty,now.step+1,1}));
		}
	}
	if(space < 15) return false;
	

	if(this->snake->length() > 12 && this->ctx->my_snakes().size() < 4) return true;
	if(this->ctx->my_snakes().size() < 3) return true;
	if(this->ctx->current_round() <= 50) return true;

	double max_safe_score = -1000;
	for(int i = 0; i < ACT_LEN; i++) max_safe_score = max(max_safe_score,this->assess->safe_score[i]);
	if(max_safe_score < this->SPLIT_THRESH) return true;

	return false;
}
bool AI::try_shoot() {
	if(!this->assess->can_shoot()) return false;
	const int tgt_type = this->target_list[this->snake->id].type;
	const pii &ass = this->assess->ray_trace();
	
	for(const Snake& enemy : this->ctx->opponents_snakes()) {
		if(this->snake->length_bank > 0) break;
		if(enemy[0].get_block_dist(this->snake->coord_list.back()) > 1) continue;
		if(this->assess->calc_snk_air(enemy[0]) > 1) continue;
		if(ass.second - ass.first + enemy.length() >= 2) {
			this->logger.log(1,"发射激光，拖延时间");
			return true;
		}
	}

	if(ass.second - ass.first >= 2 && tgt_type != 1 && tgt_type != 2) {
		this->logger.log(1,"发射激光，击毁(%d,%d)",ass.first,ass.second);
		return true;
	}

	return false;
}
int AI::try_solid() {
	//濒死
	const int leng = this->snake->length();
	const pii&& best_sol = this->assess->get_enclosing_area();//利用率（前后期）
	if(this->assess->dead_signal && (best_sol.first >= this->SOL_DYING_EFFI * leng || best_sol.first >= max(leng - 2,4))) {
		this->logger.log(1,"随缘固化(濒死)，利用%d格",best_sol.first);
		this->release_target();
		return best_sol.second;
	}

	//以下是:高效/空闲/危险
	if(this->snake->length() < this->SOL_MIN_LENG || this->ctx->my_snakes().size() < 4) return -1;

	if(best_sol.first == -1) return -1;
	if(best_sol.first >= this->SOL_HIGH_EFFI * leng) {
		this->logger.log(1,"随缘固化(高效)，利用%d格",best_sol.first);
		this->release_target();
		return best_sol.second;
	}
	if(best_sol.first >= this->SOL_IDLE_EFFI * leng && this->target_list[this->snake->id].isnull()) {
		this->logger.log(1,"随缘固化(空闲)，利用%d格",best_sol.first);
		this->release_target();
		return best_sol.second;
	}
	
	double max_safe_score = -1000;
	for(int i = 0; i < ACT_LEN; i++) max_safe_score = max(max_safe_score,this->assess->safe_score[i]);
	if(best_sol.first >= this->SOL_DANG_EFFI * leng && max_safe_score < this->SOL_SAFE_THRESH) {
		this->logger.log(1,"随缘固化(危险)，利用%d格",best_sol.first);
		this->release_target();
		return best_sol.second;
	}

	return -1;
}
int AI::do_eat() {
	const Item& item = this->ctx->find_item(this->item_addons[this->snake->id]);//动态类型引用警告
	return this->assess->find_path(Coord({item.x,item.y}));
}
int AI::do_solid() {
	target_solid& sol = this->solid_addons[this->snake->id];
	assert(sol.pos_list.size());

	const Coord& now = this->snake->coord_list[0];
	Coord tgt = sol.pos_list[0];
	int dis = now.get_block_dist(tgt);
	bool on_line = (dis == (now-tgt).get_leng());
	this->logger.log(1,"执行固化任务 -> %s",sol.pos_list[0].to_string().c_str());

	//缺一个路线阻塞的判定
	
	if(!on_line && !sol.in_error) {
		this->logger.log(1,"偏离路线,放弃固化");
		this->release_target(this->snake->id);
		return this->assess->go_safe();
	}

	if(dis == 0) {//到达目标点
		sol.pos_list.erase(sol.pos_list.begin());
		sol.in_error = false;

		assert(sol.pos_list.size());
		tgt = sol.pos_list[0];
	} else if(dis == 1 && sol.pos_list.size() == 1) {//到达终点前
		const pii&& best = this->assess->get_enclosing_area();
		if(best.first == -1) {
			this->logger.log(1,"到达指定点,围合失败");
			this->release_target(this->snake->id);
			return this->assess->go_safe();
		} else {
			this->logger.log(1,"固化完成!");
			return best.second;
		}
	} else if(dis == 1 && this->ctx->snake_map()[tgt.x][tgt.y] == sol.snkid) {
		const pii&& best = this->assess->get_enclosing_area();
		if(best.first >= sol.cover_size-2) {
			this->logger.log(1,"(非原定计划)固化完成!");
			return best.second;
		}
	}

	if(sol.pos_list.size() == 1) return this->assess->greedy_step(tgt,this->assess->DIR_ASSESS_SOLID);
	return this->assess->find_path(tgt,this->assess->DIR_ASSESS_SOLID);
}
int AI::do_kl() {
	target_kl& kl = this->kl_addons[this->snake->id];
	const Snake& tgt = this->ctx->find_snake(kl.tgt_id);

	const int tgt_dis = this->assess->get_adjcent_dis(tgt[0].x,tgt[0].y);

	if(tgt_dis <= 1 && kl.status == 0) kl.status = 1;
	this->logger.log(1,"执行进攻任务:%d",kl.status);

	const Coord&& heading = tgt[0] + (tgt[0]-tgt[1]);
	if(heading.x >= 0 && heading.y >= 0 && heading.x < 16 && heading.y < 16 && this->ctx->wall_map()[heading.x][heading.y] == -1 && this->ctx->snake_map()[heading.x][heading.y] == -1)
		if((*this->assess->dist_map[kl.killer_id])[heading.x][heading.y] != -1) return this->assess->find_path(heading,this->assess->DIR_ASSESS_KL);
	return this->assess->greedy_step(tgt[0],this->assess->DIR_ASSESS_KL);
}
int AI::do_shoot() {
	assert(this->assess->can_shoot(this->snake->id));
	this->logger.log(1,"执行融化任务");

	shoot_alloc_t& addon = this->shoot_addons[this->snake->id];
	const pii&& result = this->assess->ray_trace(this->snake->id);
	if(result.second - result.first >= addon.gain - addon.loss) {
		this->logger.log(1,"融化任务完成(或等效完成)");
		this->release_target(this->snake->id);
		return 5 - 1;
	}
	if((addon.turn_dir % 2) && this->snake->coord_list[0].x == addon.pos.x) return this->assess->find_path(addon.pos+ACT_CRD[addon.turn_dir]);
	if(addon.turn_dir % 2 == 0 && this->snake->coord_list[0].y == addon.pos.y) return this->assess->find_path(addon.pos+ACT_CRD[addon.turn_dir]);
	return this->assess->find_path(addon.pos);
}
int AI::do_dead_signal() {
	this->logger.log(1,"执行牺牲任务");
	const dead_signal sig = this->dead_signal_addons[this->snake->id];
	int tgt_id = sig.snkid;

	this->release_target();
	if(!this->ctx->inlist(tgt_id)) {
		this->logger.log(1,"目标已消失");
		return this->assess->go_safe();
	}
	
	const pii&& best = this->assess->get_enclosing_area();
	const pii&& best_tgt = this->assess->get_enclosing_leng(sig.snkid);
	if(best_tgt.first >= 0.66*(sig.leng+sig.bank) || best_tgt.first >= int(this->snake->length())-3) {
		this->logger.log(1,"不再符合救援标准");
		return this->assess->go_safe();
	}

	int dead = this->assess->find_dead();
	if(dead == -1) {
		this->logger.log(1,"任务失败");
		return this->assess->go_safe();
	}

	this->reserved_size++;
	this->release_target(tgt_id);
	this->target_list[tgt_id] = target({5,this->turn});
	this->command_addons[tgt_id] = 6;
	if(best.first != -1) return best.second;
	return dead;
}
int AI::do_command() {
	this->logger.log(1,"执行指定动作");
	
	int op = this->command_addons[this->snake->id];
	if(op < ACT_LEN) return op;
	if(op == 5 && this->assess->can_shoot(this->snake->id)) {
		this->reserved_size--;
		return op-1;
	}
	if(op == 6 && this->assess->can_split(this->snake->id)) return op-1;
	this->logger.log(1,"执行失败");
	this->release_target();
	return this->assess->go_safe();
}
void AI::release_target(int snkid, bool silent) {
	if(snkid == -1) snkid = this->snake->id;
	if(this->target_list[snkid].isnull()) return;

	int type = this->target_list[snkid].type;
	// this->logger.log(0,"abandon snkid:%d type:%d",snkid,type);
	if(type == 0) {
		assert(this->item_addons.count(snkid));
		if(!silent) this->logger.log(1,"蛇%d放弃物品目标:%s",snkid,this->ctx->find_item(this->item_addons[snkid]).to_string().c_str());
		this->item_addons.erase(snkid);
	} else if(type == 1) {
		assert(this->kl_addons.count(snkid));
		if(!silent) this->logger.log(1,"蛇%d放弃进攻目标:蛇%d",snkid,this->kl_addons[snkid].tgt_id);
		this->kl_addons.erase(snkid);
	} else if(type == 2) {
		assert(this->shoot_addons.count(snkid));
		const shoot_alloc_t& addon = this->shoot_addons[snkid];
		if(!silent) this->logger.log(1,"蛇%d放弃融墙目标:从%s向%d射击，净赚%d格",snkid,addon.pos.to_string().c_str(),addon.turn_dir,addon.gain-addon.loss);
		if(addon.turn_dir % 2) this->x_wall_alloc[addon.pos.x] = -1;
		else this->y_wall_alloc[addon.pos.y] = -1;
		this->shoot_addons.erase(snkid);
	} else if(type == 3) {
		assert(this->solid_addons.count(snkid));
		if(!silent) this->logger.log(1,"蛇%d放弃固化",snkid);
		this->solid_addons.erase(snkid);
	} else if(type == 4) {
		assert(this->dead_signal_addons.count(snkid));
		const dead_signal& sig = this->dead_signal_addons[snkid];
		this->dead_signal_alloc[sig.snkid] = -1;
		this->dead_signal_addons.erase(snkid);
	} else if(type == 5) {
		assert(this->command_addons.count(snkid));
		this->command_addons.erase(snkid);
	} else {
		printf("error type %d\n",type);
		assert(false);
	}

	this->target_list[snkid] = NULL_TARGET;
}
void AI::publish_dead_signal() {
	const int leng = this->snake->length() + this->snake->length_bank;
	if(leng < 8) return;
	if(this->ctx->my_snakes().size() < 4) return;
	if(this->target_list[this->snake->id].type == 3) return;//在固化就不管了
	
	pii&& best = this->assess->get_enclosing_area();
	// this->logger.log(0,"尝试围合:%d,%d %f %d %d %d",best.first,best.second,0.66*leng,this->snake->length()-3,best.first >= 0.66*leng,best.first >= ((this->snake->length()) - 3));
	if(best.first != -1) if(best.first >= 0.66*leng || best.first >= (this->snake->length()) - 3) return;
	
	this->logger.log(1,"求救信号发出");
	dead_signal a {this->snake->id,this->snake->length(),this->snake->length_bank,best.second,-1,this->snake->has_laser()};//为何?
	this->dead_signal_list.push_back(a);
}

//Assess类
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

	//安全区相关计算
	for(int i = 0; i < 4; i++) this->area_occupy_val[i] = 0;
	for(int x = 0; x < 16; x++) for(int y = 0; y < 16; y++) if(this->ctx.wall_map()[x][y] != -1) this->area_occupy_val[(x>=8)+2*(y<8)]++;
	for(int x = 0; x < 16; x++) for(int y = 0; y < 16; y++) if(this->ctx.snake_map()[x][y] != -1) this->area_occupy_val[(x>=8)+2*(y<8)]++;
}
void Assess::do_snake_assess() {
	this->refresh_all_bfs();
	this->calc_mixed_score();
}
void Assess::calc_mixed_score() {
	this->scan_act();
	this->calc_P_A_score();

	for(int i = 0; i < ACT_LEN; i++) this->mixed_score[i] = this->safe_score[i] + this->polite_score[i] + this->attack_score[i];
	for(int i = 0; i < ACT_LEN; i++) {
		const int tx = this->pos.x+ACT[i][0], ty = this->pos.y+ACT[i][1];
		if(tx == 15 || ty == 15 || tx == 0 || ty == 0) this->mixed_score[i] -= 2;
	}

	this->mixed_search();

	for(int i = 0; i < ACT_LEN; i++) this->dir_scores[i] = mix_score_t({this->mixed_score[i],this->search_score[i],this->act_score[i],this->safe_score[i],this->attack_score[i],this->polite_score[i]});
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
void Assess::mixed_search() {
	const int MIXED_SEARCH_DEPTH[9] = {0,7,4,2,2,1,1,1,1};
	// const int MIXED_SEARCH_DEPTH[9] = {0,2,2,2,2,1,1,1,1};
	vector<int> snk_list;
	snk_list.push_back(this->snkid);
	for(auto it = this->ctx.my_snakes().begin(); it != this->ctx.my_snakes().end(); it++) {
		if(it->id == this->snkid) continue;
		const int dst = this->get_adjcent_dis(this->pos.x,this->pos.y,it->id);
		if(dst != -1 && dst <= this->MIXED_SEARCH_SNK_DIS_THRESH[1]-1) snk_list.push_back(it->id);
	}
	for(auto it = this->ctx.opponents_snakes().begin(); it != this->ctx.opponents_snakes().end(); it++) {
		const int dst = this->get_adjcent_dis(this->pos.x,this->pos.y,it->id);
		if(dst != -1 && dst <= this->MIXED_SEARCH_SNK_DIS_THRESH[0]-1) snk_list.push_back(it->id);
	}
	this->logger.log(0,"搜索范围内有%d条蛇,初始深度为%d",snk_list.size(),MIXED_SEARCH_DEPTH[snk_list.size()]);
	Search search(this->ctx,this->logger,snk_list,this->snkid);

	int depth = MIXED_SEARCH_DEPTH[snk_list.size()];
	if(this->ai.target_list[this->snkid].type == 1) search.setup_search(depth,this->attack_val_func,5);
	else search.setup_search(depth,this->mixed_val_func,5);
	search.search();

	if(search.snk_dead) {
		this->dead_signal = true;
		this->ai.publish_dead_signal();
	}

	const search_best* ans = search.results;//更新结果
	for(int i = 0; i < ACT_MAXV; i++) this->search_score[i] = ans[i].val;
	for(int i = 0; i < ACT_LEN; i++) if(ans[i].val > -900) this->mixed_score[i] += ans[i].val * this->MIXED_SEARCH_WEIGHT;
	this->logger.log(0,"search_val:[%.2f,%.2f,%.2f,%.2f]",ans[0].val,ans[1].val,ans[2].val,ans[3].val);
}
double Assess::mixed_val_func(const Context& begin, const Context& end, int snkid) {
	double ans = 0;
	const pii&& snake_begin = begin.calc_snake_leng();
	const pii&& snake_end = end.calc_snake_leng();
	const pii&& wall_begin = begin.calc_wall();
	const pii&& wall_end = end.calc_wall();
	if(begin.find_snake(snkid).camp == 0) {
		ans += 0.5737 * (snake_end.first - snake_begin.first);
		ans += 0.1565 * (wall_end.first - wall_begin.first);
		ans += 0.2339 * (snake_begin.second - snake_end.second);
		ans += 0.0360 * (wall_begin.second - wall_end.second);
	} else {
		ans += 0.5737 * (snake_end.second - snake_begin.second);
		ans += 0.1565 * (wall_end.second - wall_begin.second);
		ans += 0.2339 * (snake_begin.first - snake_end.first);
		ans += 0.0360 * (wall_begin.first - wall_end.first);
	}
	return ans;
}
double Assess::attack_val_func(const Context& begin, const Context& end, int snkid) {
	double ans = 0;
	const pii&& snake_begin = begin.calc_snake_leng();
	const pii&& snake_end = end.calc_snake_leng();
	if(!end.inlist(snkid)) ans -= 1;
	if(begin.find_snake(snkid).camp == 0) {
		ans += 0.5 * (snake_end.first - snake_begin.first);
		ans += 0.6 * (snake_begin.second - snake_end.second);
	} else {
		ans += 0.5 * (snake_end.second - snake_begin.second);
		ans += 0.6 * (snake_begin.first - snake_end.first);
	}
	return ans;
	
}
void Assess::scan_act() {
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
		this->__scan_act_bfs(i);

		const double leng = this->this_snake.length() + max(this->this_snake.length_bank,2);
		if(this->act_score[i] <= this->CRIT_AIR_PARAM[0] || this->act_score[i] <= min(leng*this->CRIT_AIR_PARAM[1],15.0))
			this->safe_score[i] += this->CRIT_AIR_PARAM[2] + leng*this->CRIT_AIR_PARAM[3] + this->act_score[i]*this->CRIT_AIR_PARAM[4];
		else if(this->act_score[i] <= this->LOW_AIR_PARAM[0] || this->act_score[i] <= min(leng*this->LOW_AIR_PARAM[1],20.0))
			this->safe_score[i] += min(0.0,(max(this->LOW_AIR_PARAM[0],min(leng*this->LOW_AIR_PARAM[1],20.0))-this->act_score[i])*this->LOW_AIR_PARAM[2] + this->act_score[i]*this->LOW_AIR_PARAM[3]);
		
		if(this->safe_score[i] > -90 && this->safe_score[i] < -0.5) will_log = true;
	}

	if(will_log) {
		this->logger.log(0,"act_score:[%.1f,%.1f,%.1f,%.1f]",this->act_score[0],this->act_score[1],this->act_score[2],this->act_score[3]);
		this->logger.log(0,"safe_score:[%.1f,%.1f,%.1f,%.1f]",this->safe_score[0],this->safe_score[1],this->safe_score[2],this->safe_score[3]);
	}
}
void Assess::__scan_act_bfs(int actid) {
	const int rx = this->pos.x,ry = this->pos.y;
	const vector<int> &&bank_list = this->bank_siz_list();
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
			if(!this->check_nstep_norm(tx,ty,now.step+1,this->snkid,bank_list[now.step]) || this->temp_map_int[tx][ty]) continue;
			const double heads = this->__calc_head_pena(tx,ty,this->snkid);
			this->temp_map_int[tx][ty] = 1;
			this->act_score[actid] += now.val*heads;
			qu.push(scan_act_q_t({tx,ty,now.step+1,now.val*heads}));
		}
	}
}
double Assess::__calc_head_pena(int nx, int ny, int snkid) {
	double ans = 1;
	const Coord now = Coord({nx,ny});
	const int camp = this->ctx.find_snake(snkid).camp;
	for(int i = 0; i < ACT_LEN; i++) {//计算"在头旁边"的部分
		int tx = nx + ACT[i][0], ty = ny + ACT[i][1];
		if(tx < 0 || ty < 0 || tx >= 16 || ty >= 16) continue;

		int snk = this->ctx.snake_map()[tx][ty];//不是自己的头
		if(snk != -1 && snk != snkid && Coord({tx,ty}) == this->ctx.find_snake(snk)[0]) {
			if(this->ctx.find_snake(snk).camp == camp) ans *= this->SCAN_ACT_NEAR_REDUCE[1];
			else ans *= this->SCAN_ACT_NEAR_REDUCE[0];
		}
	}

	//计算"方向"部分
	// for(auto it = this->ctx.my_snakes().begin(); it != this->ctx.my_snakes().end(); it++) if(it->id != this->snkid && (*this->dist_map[it->id])[nx][ny] == 2 && this->trace_head_dir(2,it->id) == now) ans *= this->SCAN_ACT_DIRE_REDUCE[1];
	// for(auto it = this->ctx.opponents_snakes().begin(); it != this->ctx.opponents_snakes().end(); it++) if((*this->dist_map[it->id])[nx][ny] == 2 && this->trace_head_dir(2,it->id) == now) ans *= this->SCAN_ACT_DIRE_REDUCE[0];

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
			// if(ftr < curr) this->attack_score[i] += leng*0.1;//闭气奖励，或许是limit-2的前身
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

}
int Assess::get_bfs_dis(const Coord& tgt, int snkid) {
	if(snkid == -1) snkid = this->snkid;
	return (*this->dist_map[snkid])[tgt.x][tgt.y];
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
int Assess::greedy_step(const Coord &tgt, double (*dir_assess)(int ind, bool directed, const mix_score_t& scores)) {
	act_score_t greedy_list[4];
	const int x = this->pos.x, y = this->pos.y;
	const int dx = tgt.x-x, dy = tgt.y-y;

	bool directed[ACT_LEN] = {false,false,false,false};
	if(dx > 0) directed[0] = true;
	if(dx < 0) directed[2] = true;
	if(dy > 0) directed[1] = true;
	if(dy < 0) directed[3] = true;
	for(int i = 0; i < ACT_LEN; i++) if(directed[i] && this->mixed_score[i] <= -100) directed[i] = false;//可行性检验
	
	bool no_dir = true;
	for(int i = 0; i < ACT_LEN; i++) if(directed[i]) no_dir = false;
	if(no_dir) {
		int best_i = -1;
		double best_v = 1000;
		for(int i = 0; i < ACT_LEN; i++) if(this->mixed_score[i] > -100 && (this->pos+ACT_CRD[i]-tgt).get_leng() < best_v) best_i = i, best_v = (this->pos+ACT_CRD[i]-tgt).get_leng();
		if(best_i != -1) directed[best_i] = true;
	}

	for(int i = 0; i < ACT_LEN; i++) greedy_list[i] = act_score_t({i,(*dir_assess)(i,directed[i],this->dir_scores[i])});
	sort(&greedy_list[0],&greedy_list[ACT_LEN],act_score_cmp);

	if(greedy_list[0].val < -80) return this->emergency_handle();
	this->logger.log(1,"贪心寻路:%d 目标:(%2d,%2d)",greedy_list[0].actid,tgt.x,tgt.y);
	return greedy_list[0].actid;
}
int Assess::find_path(const Coord &tgt, double (*dir_assess)(int ind, bool directed, const mix_score_t& scores)) {
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
	const int selected = this->rev_step(rev);
	int sel_dis = (this->pos+ACT_CRD[selected]).get_block_dist(tgt);
	if(this->get_bfs_dis(tgt) > this->pos.get_block_dist(tgt)) sel_dis = -10;
	for(int i = 0; i < ACT_LEN; i++) bfs_list[i] = act_score_t({i,(*dir_assess)(i,i==selected || sel_dis == (this->pos+ACT_CRD[i]).get_block_dist(tgt),this->dir_scores[i])});
	// for(int i = 0; i < ACT_LEN; i++) bfs_list[i] = act_score_t({i,(*dir_assess)(i,i==selected,this->dir_scores[i])});
	sort(&bfs_list[0],&bfs_list[ACT_LEN],act_score_cmp);

	if(bfs_list[0].actid != this->rev_step(rev)) this->logger.log(1,"寻路[非原向]:%d 目标:(%2d,%2d)",bfs_list[0].actid,tgt.x,tgt.y);
	else this->logger.log(1,"寻路:%d 目标:(%2d,%2d)",bfs_list[0].actid,tgt.x,tgt.y);
	return bfs_list[0].actid;
}
// int Assess::go_safe() {
// 	this->logger.log(1,"尝试疏散");

// 	const Coord& tgt = this->ai.safe_area_alloc[this->snkid];
// 	if(tgt == NULL_COORD) {
// 		if(this->this_snake.length() > 5) this->logger.log(1,"未分配到安全区");
// 		else this->logger.log(1,"长度过短,不分配安全区");
// 		return this->random_step();
// 	}

// 	if(this->get_bfs_dis(tgt) == -1) return this->greedy_step(tgt,this->DIR_ASSESS_GO_SAFE);
// 	else return this->find_path(tgt,this->DIR_ASSESS_GO_SAFE);
// }
int Assess::go_safe() {
	int best = 0, now = (this->this_snake[0].x>=8) + 2*(this->this_snake[0].y<8);
	for(int i = 1; i < 4; i++) if(this->area_occupy_val[i] < this->area_occupy_val[best]) best = i;
	if(now == best) return this->random_step();

	this->logger.log(1,"走向安全区%d",best);
	this->area_occupy_val[best] += max(10,min(20,int(this->this_snake.length()) + this->this_snake.length_bank));
	for(int x = 0; x < 16; x++) 
		for(int y = 0; y < 16; y++) 
			if((x>=8)+2*(y<8) == best && (*this->dist_map[this->snkid])[x][y] != -1) return this->find_path(Coord({x,y}),this->DIR_ASSESS_GO_SAFE);
	
	//估计是路都被堵住了...
	return this->random_step();
}
double Assess::GRED_ASSESS_REGULAR(int ind, bool directed, const mix_score_t& scores) {
	double ans = scores.mixed_score;
	if(directed) ans += 4;
	return ans;
}
double Assess::DIR_ASSESS_REGULAR(int ind, bool directed, const mix_score_t& scores) {
	double ans = scores.mixed_score;
	if(directed) ans += 6;
	return ans;
}
double Assess::DIR_ASSESS_SOLID(int ind, bool directed, const mix_score_t& scores) {
	double ans = scores.polite_score + scores.search_score + scores.safe_score * 0.5;
	if(directed) ans += 12;
	return ans;
}
double Assess::DIR_ASSESS_GO_SAFE(int ind, bool directed, const mix_score_t& scores) {
	double ans = scores.mixed_score;
	if(directed) ans += 2;
	return ans;
}
double Assess::DIR_ASSESS_KL(int ind, bool directed, const mix_score_t& scores) {
	double ans = scores.polite_score * 0.5 + scores.search_score*3 + scores.attack_score + scores.safe_score * 0.5;
	if(directed) ans += 4;
	return ans;
}
int Assess::emergency_handle() {
	//先尝试发激光
	bool shoot_useful = false;
	for(int i = 0; i < ACT_LEN; i++) {
		const Coord&& next = this->pos + ACT_CRD[i];
		bool front = (this->this_snake.length() > 1) && (ACT_CRD[i] == this->pos - this->this_snake[1]);

		if(next.x < 0 || next .y < 0 || next.x >= MAP_LENGTH || next.y >= MAP_LENGTH) continue;
		if(front && this->ctx.wall_map()[next.x][next.y] != -1) shoot_useful = true;
		
		int blocking_snake = this->ctx.snake_map()[next.x][next.y];
		if(blocking_snake != -1 && blocking_snake != this->snkid && this->get_pos_on_snake(next) <= 2) shoot_useful = true;//不完全uesful
	}

	if(this->can_shoot() && shoot_useful) {
		pii &&rt = this->ray_trace();
		if(rt.first-rt.second <= this->EM_RAY_COST[0] && rt.first-rt.second <= this->this_snake.length()*this->EM_RAY_COST[1]) {
			this->logger.log(1,"紧急处理:发射激光，击毁(%d,%d)",rt.first,rt.second);
			return 5 - 1;
		}
	}

	//找到不犯规的走法
	vector<int> valid;
	const pii&& best = this->get_enclosing_area();
	for(int i = 0; i < ACT_LEN; i++) {
		const int tx = this->pos.x+ACT[i][0], ty = this->pos.y+ACT[i][1];
		if(tx < 0 || ty < 0 || tx >= 16 || ty >= 16) valid.push_back(i);
		else if(this->ctx.snake_map()[tx][ty] != this->snkid) valid.push_back(i);
	}
	//尝试固化（蛇）
	if(best.first >= this->EM_SOLID_EFF*(this->this_snake.length()+this->this_snake.length_bank) && this->ctx.my_snakes().size() >= this->EM_SOLID_COUNT_MIN) {
		this->logger.log(1,"紧急处理:被动固化，利用%d格蛇长",best.first);
		this->ai.release_target();
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
		this->ai.release_target();
		return valid[0];
	}
	this->logger.log(1,"紧急处理:被动固化，利用%d格蛇长",best.first);
	this->ai.release_target();
	return best.second;
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

	const Snake &snk = this->ctx.find_snake(snkid);//引用警告
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
pii Assess::get_enclosing_area() {
	pii best = pii({-1,-1});
	const Snake &snk = this->ctx.find_snake(snkid);
	for(int i = 0; i < ACT_LEN; i++) {
		const int tx = snk[0].x+ACT[i][0], ty = snk[0].y+ACT[i][1];
		if(tx < 0 || ty < 0 || tx >= 16 || ty >= 16) continue;
		if(this->ctx.snake_map()[tx][ty] != snkid) continue;
		if(Coord({tx,ty}) == snk.coord_list.back() && snk.length_bank == 0) continue;
		if(snk.length() > 2 && tx == snk[1].x && ty == snk[1].y) continue;
		if(snk.length() == 2 && tx == snk[1].x && ty == snk[1].y && snk.length_bank > 0) continue;

		Context tmp = this->ctx;
		tmp.do_operation(Operation({i+1}));
		const pii&& wall_begin = this->ctx.calc_wall();
		const pii&& wall_end = tmp.calc_wall();

		int area = 0;
		if(this->camp == 0) area = (wall_end.first - wall_begin.first) + (wall_begin.second - wall_end.second);
		else area = (wall_end.second - wall_begin.second) + (wall_begin.first - wall_end.first);
		if(area > best.first) best = pii({area,i});
	}
	return best;
}
bool Assess::check_go_straight(int snkid, int leng) {
	if(snkid == -1) snkid = this->snkid;

	const Snake& snk = this->ctx.find_snake(snkid);
	if(snk.length() < leng || leng < 2) return false;

	const Coord &pos0 = snk[0], &pos1 = snk[1];
	const Coord&& dire = pos1 - pos0;//反蛇头方向的dire
	for(int i = 3; i <= leng; i++) if(snk[i-1].x != pos0.x + (i-1)*dire.x || snk[i-1].y != pos0.y + (i-1)*dire.y) return false;
	return true;
}
double Assess::get_encounter_angle(int attacker, int target) {
	const Snake& tgt = this->ctx.find_snake(target);
	return (tgt[0] - this->ctx.find_snake(attacker)[0]).get_angle(tgt[0] - tgt[1]);
}
int Assess::get_adjcent_dis(int x, int y, int snkid) {
	if(snkid == -1) snkid = this->snkid;
	int ans = (*this->dist_map[snkid])[x][y];
	for(int i = 0; i < ACT_LEN; i++) {
		const int tx = x+ACT[i][0], ty = y+ACT[i][1];
		if(tx < 0 || ty < 0 || tx >= MAP_LENGTH || ty >= MAP_LENGTH) continue;
		const int dst = (*this->dist_map[snkid])[tx][ty];
		if(ans == -1) ans = dst;
		else ans = min(ans,dst);
	}
	return ans;
}
Coord Assess::trace_head_dir(int dist, int snkid) {
	if(snkid == -1) snkid = this->snkid;
	const Snake& snk = this->ctx.find_snake(snkid);

	if(snk.length() == 1) return NULL_COORD;

	const Coord pos0 = snk[0], pos1 = snk[1];
	const Coord&& dire = pos0 - pos1;
	const Coord ans = Coord({pos0.x+dist*dire.x,pos0.y+dist*dire.y});
	if(ans.x < 0 || ans.y < 0 || ans.x >= this->x_leng || ans.y >= this->y_leng) return NULL_COORD;
	return ans;
}
bool Assess::check_path_clear(const Coord& start, const Coord& end, int snkid) {
	int dis = start.get_block_dist(end);
	assert(dis == (start-end).get_leng() && dis > 0);

	const Coord dire = Coord({(end.x-start.x)/dis,(end.y-start.y)/dis});
	for(Coord now = start; now != end+dire; now = now + dire) {
		if(this->ctx.wall_map()[now.x][now.y] != -1) return false;
		if(this->ctx.snake_map()[now.x][now.y] != -1 && this->ctx.snake_map()[now.x][now.y] != snkid) return false;
	}
	return true;
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
int Assess::find_dead(int snkid) {
	if(snkid == -1) snkid = this->snkid;
	const Snake& snake = this->ctx.find_snake(snkid);

	for(int i = 0; i < ACT_LEN; i++) {
		Coord&& next = snake[0] + ACT_CRD[i];
		if(next.x < 0 || next.y < 0 || next.x >= MAP_LENGTH || next.y >= MAP_LENGTH) return i;
		if(this->ctx.wall_map()[next.x][next.y] != -1) return i;

		int blocking_snk = this->ctx.snake_map()[next.x][next.y];
		if(blocking_snk != -1 && blocking_snk != snkid) return i;
		if(blocking_snk == snkid && snake.length() >= 2)
			if(next != snake[1] && (next != snake.coord_list.back() || snake.length_bank)) return i;
	}
	return -1;
}
pii Assess::ray_trace(int snkid) {
	if(snkid == -1) snkid = this->snkid;
	const Snake &snk = this->ctx.find_snake(snkid);
	if(snk.length() < 2) return pii({-1,-1});

	const Coord pos0 = snk[0],pos1 = snk[1];
	return this->ray_trace_dir(pos0,pos0-pos1);
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