#include <algorithm>
#include <cassert>
#include <utility>
#include <map>
#include <queue>
#include <vector>
#include "adk.hpp"
#include "logging.cpp"
using namespace std;

#define MAP_LENGTH 16

typedef pair<int,int> pii;
typedef int map_int_t[MAP_LENGTH][MAP_LENGTH];

const int ACT_LEN = 4;
const int ACT[4][2] = {{1,0},{0,1},{-1,0},{0,-1}};
int temp_map_int[MAP_LENGTH][MAP_LENGTH];//【这行不行呢？】

struct act_score_t {
	int actid;
	double val;
};
bool act_cmp(const act_score_t &a, const act_score_t &b) {
	return a.val > b.val;//降序【可能不对】
}
struct spd_map_t {
	int dist;
	int snkid;

	bool operator==(const spd_map_t &b) {return dist == b.dist && snkid == b.snkid;}
	bool operator!=(const spd_map_t &b) {return !((*this) == b);}
};
struct find_path_q_t {
	int x,y,step;
};
struct scan_act_q_t {
	int x,y,step;
	double val;
};

class AI;
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
		

		int rev_step(int st);
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

		constexpr static spd_map_t SPD_MAP_NULL = spd_map_t({-1,-1});
		//find_path系列常数
		const static int BANK_SIZ_LIST_SIZE = 100;
		//scan_act系列常数
		const static int SCAN_ACT_MAX_DEPTH = 5;
		constexpr static double SCAN_ACT_REDUCE_FACTOR[2] = {0.2,0.4};//敌,我
		constexpr static double CRIT_AIR_PARAM[5] = {4.5,0.75,-8.0,-1.0,0.3};//(上限绝对值,上限比例,惩罚绝对值,惩罚leng系数,奖励score系数)
		constexpr static double LOW_AIR_PARAM[4] = {10,2.5,-0.8,0.2};//(上限绝对值,上限比例,惩罚系数,奖励score系数)
		//PA分系列常数
		constexpr static double P_1_AIR_PARAM[2] = {-4,-0.4};
		constexpr static double P_NO_AIR_PARAM[2] = {-8,-1};
		constexpr static double A_AIR_MULT[2] = {0.7,0.3};//(0 air,1 air)
		constexpr static double A_ENV_BONUS[2] = {2,2};//(No laser, 4 snakes)
		constexpr static double A_SMALL_BONUS[3] = {0,1.2,1.5};

		constexpr static int REV_PARAM[4] = {2,3,0,1};
		constexpr static double GREEDY_DIR_SCORE = 4.0;
		constexpr static double FNDPTH_DIR_SCORE = 6.0;
		constexpr static double EM_RAY_COST[2] = {2,1.0/3};
		constexpr static double EM_SOLID_EFF = 0.75;
};

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
			friend_spd[x][y] = this->SPD_MAP_NULL;
			enemy_spd[x][y] = this->SPD_MAP_NULL;
			tot_spd[x][y] = this->SPD_MAP_NULL;
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
				if(this->friend_spd[x][y] == this->SPD_MAP_NULL) this->friend_spd[x][y] = spd_map_t({dst,it->id});
				else if(this->friend_spd[x][y].dist > dst) this->friend_spd[x][y] = spd_map_t({dst,it->id});
			}
			for(auto it = this->ctx.opponents_snakes().begin(); it != this->ctx.opponents_snakes().end(); it++) {
				int dst = (*this->dist_map[it->id])[x][y];
				if(dst == -1) continue;
				if(this->enemy_spd[x][y] == this->SPD_MAP_NULL) this->enemy_spd[x][y] = spd_map_t({dst,it->id});
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
	bool will_log = false;
	const int x = this->pos.x,y = this->pos.y;

	for(int i = 0; i < ACT_LEN; i++) this->act_score[i] = this->safe_score[i] = 0.0;
	for(int i = 0; i < ACT_LEN; i++) {
		const int tx = x+ACT[i][0],ty = y+ACT[i][1];
		if(!this->check_nstep_norm(tx,ty)) {
			this->act_score[i] = this->safe_score[i] = -100.0;
			continue;
		}
		if(this->this_snake.length() <= 2) {
			this->act_score[i] = 25.0;
			continue;
		}
		this->scan_act_bfs(i);

		const double leng = this->this_snake.length() + this->this_snake.length_bank;
		if(this->act_score[i] <= this->CRIT_AIR_PARAM[0] || this->act_score[i] <= leng*(this->CRIT_AIR_PARAM[1]))
			this->safe_score[i] += this->CRIT_AIR_PARAM[2] + leng*this->CRIT_AIR_PARAM[3] + this->act_score[i]*this->CRIT_AIR_PARAM[4];
		else if(this->act_score[i] <= this->LOW_AIR_PARAM[0] || this->act_score[i] <= leng*(this->LOW_AIR_PARAM[1]))
			this->safe_score[i] += min(0,(max(this->LOW_AIR_PARAM[0],leng*(this->LOW_AIR_PARAM[1]))-this->act_score[i])*this->LOW_AIR_PARAM[2] + this->act_score[i]*this->LOW_AIR_PARAM[3]);
		
		if(this->safe_score[i] > -90 && this->safe_score[i] < -0.5) will_log = true;
	}

	if(will_log) {
		this->logger.log(0,"act_score:[%.1f,%.1f,%.1f,%.1f]",this->act_score[0],this->act_score[1],this->act_score[2],this->act_score[3]);
		this->logger.log(0,"safe_score:[%.1f,%.1f,%.1f,%.1f]",this->safe_score[0],this->safe_score[1],this->safe_score[2],this->safe_score[3]);
	}
}
void Assess::scan_act_bfs(int actid) {
	const int rx = this->pos.x,ry = this->pos.y;

	for(int x = 0; x < this->x_leng; x++) for(int y = 0; y < this->y_leng; y++) temp_map_int[x][y] = 0;
	temp_map_int[rx][ry] = 1,temp_map_int[rx+ACT[actid][0]][ry+ACT[actid][1]] = 1;

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
			temp_map_int[tx][ty] = 1;
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
	const int x = this->pos.x, y = this->pos.y;
	bool will_log_atk = false, will_log_pol = false;
	for(int i = 0; i < ACT_LEN; i++) this->attack_score[i] = this->polite_score[i] = -100;

	for(int i = 0; i < ACT_LEN; i++) {
		const int tx = x+ACT[i][0], ty = y+ACT[i][1];
		if(!this->check_nstep_norm(tx,ty)) continue;
		this->attack_score[i] = this->polite_score[i] = 0;

		Coord extra_go = Coord({-1,-1});
		if(!this->this_snake.length_bank) extra_go = this->this_snake.coord_list.back();
		for(auto it = this->ctx.my_snakes().begin(); it != this->ctx.my_snakes().end(); it++) {
			if(it->id == this->snkid) continue;
			const int leng = it->length() + it->length_bank;
			const int curr = this->calc_snk_air(it->coord_list[0]);
			const int ftr = this->calc_snk_air(it->coord_list[0],Coord({tx,ty}),extra_go);
			if(ftr < curr && ftr == 1) this->polite_score[i] += this->P_1_AIR_PARAM[0] + leng*this->P_1_AIR_PARAM[1];
			if(ftr < curr && ftr == 0) this->polite_score[i] += this->P_NO_AIR_PARAM[0] + leng*this->P_NO_AIR_PARAM[1];
		}
		for(auto it = this->ctx.opponents_snakes().begin(); it != this->ctx.opponents_snakes().end(); it++) {
			const int leng = it->length() + it->length_bank;
			const int curr = this->calc_snk_air(it->coord_list[0]);
			const int ftr = this->calc_snk_air(it->coord_list[0],Coord({tx,ty}),extra_go);
			if(ftr < curr && ftr == 1) this->attack_score[i] += leng*this->A_AIR_MULT[1];
			if(ftr < curr && ftr == 0) {
				this->attack_score[i] += leng*this->A_AIR_MULT[0];
				if(!this->can_shoot(it->id)) this->attack_score[i] += this->A_ENV_BONUS[0];
				if(!this->can_split(it->id)) this->attack_score[i] += this->A_ENV_BONUS[1];
			}
		}
		if(this->attack_score[i] > 0.5) will_log_atk = true;
		if(this->polite_score[i] > -90 && this->polite_score[i] < -0.5) will_log_pol = true;
	}
	const int leng = this->this_snake.length() + this->this_snake.length_bank;
	if(leng <= 2) for(int i = 0; i < ACT_LEN; i++) this->attack_score[i] *= this->A_SMALL_BONUS[leng];

	if(will_log_atk) this->logger.log(0,"atk_score:[%.1f,%.1f,%.1f,%.1f]",this->attack_score[0],this->attack_score[1],this->attack_score[2],this->attack_score[3]);
	if(will_log_pol) this->logger.log(0,"pol_score:[%.1f,%.1f,%.1f,%.1f]",this->polite_score[0],this->polite_score[1],this->polite_score[2],this->polite_score[3]);
}
void Assess::find_path_bfs(int snkid = -1) {
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
vector<int> Assess::bank_siz_list(int snkid = -1) {
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
int Assess::rev_step(int st) {
	return this->REV_PARAM[st];
}
int Assess::random_step() {
	act_score_t random_list[4];
	for(int i = 0; i < ACT_LEN; i++) random_list[i] = act_score_t({i,this->mixed_score[i]});
	sort(&random_list[0],&random_list[ACT_LEN]);

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
	sort(&greedy_list[0],&greedy_list[ACT_LEN]);

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
	sort(&bfs_list[0],&bfs_list[ACT_LEN]);

	if(bfs_list[0].actid != this->rev_step(rev))
		this->logger.log(1,"寻路[非原向]:%d 目标:(%2d,%2d)",bfs_list[0].actid,tgt.x,tgt.y);
	else this->logger.log(1,"寻路:%d 目标:(%2d,%2d)",bfs_list[0].actid,tgt.x,tgt.y);
	return bfs_list[0].actid;
}
int Assess::emergency_handle() {
	//先尝试发激光
	if(this->can_shoot()) {
		pii &rt = this->ray_trace();
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
	//尝试固化
	if(best.first >= this->EM_SOLID_EFF*(this->this_snake.length()+this->this_snake.length_bank)) {
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
void Assess::release_target(int snkid = -1) {
	if(snkid == -1) snkid = this->snkid;
	//
}
vector<Item> Assess::get_captured_items(int snkid = -1, int item_tp = -1) {
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
bool Assess::check_item_captured(const Item &item, int snkid = -1) {
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
int Assess::calc_snk_air(const Coord &pos,const Coord &extra_block = Coord({-1,-1}),const Coord &extra_go = Coord({-1,-1})) {
	int ans = 0;
	const int x = this->pos.x, y = this->pos.y;
	const int snkid = this->ctx.snake_map()[x][y];

	for(int i = 0; i < ACT_LEN; i++) {
		const int tx = x+ACT[i][0], ty = y+ACT[i][1];
		const Coord t_pos = Coord({tx,ty});
		if(t_pos == extra_block) continue;
		if(this->check_nstep_norm(tx,ty,1,snkid)) ans++;
		else if(t_pos == extra_go) ans++;
	}
	return ans;
}
pii Assess::get_enclosing_leng(int snkid = -1) {
	if(snkid == -1) snkid = this->snkid;
	pii best = pii({-1,-1});
	const Snake &snk = this->ctx.find_snake(snkid);
	for(int i = 0; i < ACT_LEN; i++) {
		const int tx = snk[0].x+ACT[i][0], ty = snk[0].y+ACT[i][1];
		if(tx < 0 || ty < 0 || tx >= 16 || ty >= 16) continue;
		if(this->ctx.snake_map()[tx][ty] != snkid) continue;
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
bool Assess::check_nstep_norm(int tx, int ty, int step = 1, int snkid = -1, int self_bloc_bank_val = -1) {
	if(snkid == -1) snkid = this->snkid;

	if(tx < 0 || ty < 0 || tx >= 16 || ty >= 16 || this->ctx.wall_map()[tx][ty] != -1) return false;//越界/撞墙

	const int blocking_snake = this->ctx.snake_map()[tx][ty];
	if(blocking_snake == -1) return true;

	//仅在self_blocking时采用override
	if(self_bloc_bank_val == -1) self_bloc_bank_val = this->ctx.find_snake(blocking_snake).length_bank;
	else if(blocking_snake != this->snkid) self_bloc_bank_val = this->ctx.find_snake(blocking_snake).length_bank;
	const int leave_time = this->get_pos_on_snake(Coord({tx,ty})) + self_bloc_bank_val;

	if(blocking_snake == snkid) {//self_blocking
		if(leave_time <= step) return true;
		return false;
	}

	if(leave_time < step) return true;
	if(leave_time > step) return false;
	if(this->check_first(snkid,blocking_snake)) return false;
	return true;
}
bool Assess::can_split(int snkid = -1) {
	if(snkid == -1) snkid = this->snkid;
	if(this->ctx.my_snakes().size() >= 4) return false;
	if(this->ctx.find_snake(snkid).length()) return false;
	return true;
}
bool Assess::has_laser(int snkid = -1) {
	if(snkid == -1) snkid = this->snkid;
	return this->ctx.find_snake(snkid).has_laser();
}
bool Assess::can_shoot(int snkid = -1) {
	if(snkid == -1) snkid = this->snkid;
	if(!this->has_laser(snkid) || this->ctx.find_snake(snkid).length() < 2) return false;
	return true;
}
pii Assess::ray_trace(int snkid = -1) {
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