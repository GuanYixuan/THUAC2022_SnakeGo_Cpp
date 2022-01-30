#include <cassert>
#include <vector>
#include "adk.hpp"
using namespace std;

const int MAP_LENGTH = 16;
const int ACT[4][2] = {{1,0},{0,1},{-1,0},{0,-1}};

class Assess {
	public:
		Assess(const Context &ctx, int snkid);
		//calc_spd_map
		//scan_act
		
		int rev_step(int st);
		int random_step();//
		int greedy_step();//
		int emergency_handle();//
		int find_first(const Coord tgt);
		void refresh_all_bfs();


		void release_target(int snkid = -1);//
		//__calc_P_A_score
		int calc_snk_air(const Coord pos,const Coord extra_block = Coord({-1,-1}),const Coord extra_go = Coord({-1,-1}));
		//get_captured_items
		//检查物品是否被哪方的蛇占住了（可以用身子直接吃掉），没有则返回-1
		int check_item_captured_team(const Item &item);
		//检查物品是否已经被snkid的蛇占住了（可以用身子直接吃掉）
		bool check_item_captured(const Item &item, int snkid = -1);
		//返回pos格是蛇上的第几格，需保证pos格有蛇
		int get_pos_on_snake(const Coord pos);

		//检查编号为first的蛇的下一次行动是否比编号为second的蛇先，【这一判断基于目前正在行动的蛇的id（即self.snkid）作出】
		bool check_first(int first, int second);
		//判断id=snkid的蛇在接下来的第step步移动后走到(tx,ty)这一格是否不会被撞死
		bool check_nstep_norm(int tx, int ty, int step = 1, int snkid = -1, int self_bloc_bank_val = -1);

		bool can_split(int snkid = -1);
		bool has_laser(int snkid = -1);
		bool can_shoot(int snkid = -1);
		//ray_trace_self
		//ray_trace
	
	private:
		const Context &ctx;
		//snakes : "list[Snake]"
		const Snake &this_snake;
		//game_map : Map
		const int x_leng = MAP_LENGTH;
		const int y_leng = MAP_LENGTH;
		const int snkid;
		const int camp;
		//pos : "tuple[int,int]"

		
		const static int SCAN_ACT_MAX_DEPTH = 6;
		constexpr static double SCAN_ACT_REDUCE_FACTOR[2] = {0.2,0.3};


		constexpr static int REV_PARAM[4] = {2,3,0,1};
		constexpr static double GREEDY_DIR_SCORE = 4.0;

};

Assess::Assess(const Context &ctx, int snkid) :
ctx(ctx),
this_snake(ctx.find_snake(snkid)),
snkid(snkid),
camp(ctx.current_player())
{

}
int Assess::rev_step(int st) {
	return this->REV_PARAM[st];
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
int Assess::get_pos_on_snake(const Coord pos) {
	const int x = pos.x,y = pos.y;
	const Snake &snk = this->ctx.find_snake(this->ctx.snake_map()[x][y]);
	for(int i = 0; i < snk.length(); i++) if(snk[i] == pos) return snk.length()-i;
	assert(false);
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
// bool Assess::has_laser(int snkid = -1) {
// 	if(snkid == -1) snkid = this->snkid;
// 	if(this->ctx.find_snake(snkid).railgun_item)
// }