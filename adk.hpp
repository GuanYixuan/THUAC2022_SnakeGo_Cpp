#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <queue>
#include <set>
#include <string>
#include <vector>

/*   Channel API Begins   */

class Channel
{
public:
	virtual ~Channel() = default;
	virtual bool send( char* msg, size_t len ) = 0;
	virtual bool recv( char* buf, size_t len ) = 0;
};

Channel* socket_channel( const std::string& host, unsigned int port );
Channel* stdio_channel();

#if defined( __unix__ ) || ( defined( __APPLE__ ) && defined( __MACH__ ) ) || defined( __CYGWIN__ )
#define ADK_POSIX
#elif defined( WIN32 ) || defined( _WIN32 ) || defined( __WIN32__ ) || defined( __NT__ )
#define ADK_WIN
#endif

/*   Channel API Ends   */

/*   Posix TCP Socket Channel Impl Begins   */

#ifdef ADK_POSIX

#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

class PosixSocket : public Channel
{
public:
	explicit PosixSocket( int internal_socket );
	~PosixSocket() override;
	bool send( char* msg, size_t len ) override;
	bool recv( char* buf, size_t len ) override;

private:
	int internal_socket;
};

inline PosixSocket::PosixSocket( int internal_socket ) : internal_socket( internal_socket ) {}

inline PosixSocket::~PosixSocket() { close( internal_socket ); }

inline bool PosixSocket::send( char* msg, size_t len ) { return ::send( internal_socket, msg, len, 0 ) != -1; }

inline bool PosixSocket::recv( char* buf, size_t len ) { return ::recv( internal_socket, buf, len, 0 ) == len; }

inline Channel* socket_channel( const std::string& host, unsigned int port )
{
	int sock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if ( sock == -1 )
		return nullptr;

	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_port = htons( port );
	addr.sin_addr.s_addr = inet_addr( host.c_str() );
	if ( connect( sock, (struct sockaddr*) &addr, sizeof( addr ) ) == -1 )
		return nullptr;

	return new PosixSocket( sock );
}

#endif // ADK_POSIX

/*   Posix TCP Socket Channel Impl Ends   */

/*   Winsock2 Channel Impl Begins   */

#ifdef ADK_WIN

#pragma comment( lib, "ws2_32.lib" )

#include <WS2tcpip.h>
#include <WinSock2.h>

class WindowsSocket : public Channel
{
public:
	WindowsSocket( SOCKET internal_socket );
	virtual ~WindowsSocket();
	virtual bool send( char* msg, size_t len ) override;
	virtual bool recv( char* buf, size_t len ) override;

private:
	SOCKET internal_socket;
};

inline WindowsSocket::WindowsSocket( SOCKET internal_socket ) : internal_socket( internal_socket ) {}

inline WindowsSocket::~WindowsSocket()
{
	shutdown( internal_socket, SD_SEND );
	closesocket( internal_socket );
	WSACleanup();
}

inline bool WindowsSocket::send( char* msg, size_t len )
{
	return ::send( internal_socket, msg, len, 0 ) != SOCKET_ERROR;
}

inline bool WindowsSocket::recv( char* buf, size_t len ) { return ::recv( internal_socket, buf, len, 0 ) == len; }

inline Channel* socket_channel( const std::string& host, unsigned int port )
{
	WSADATA wsaData;
	int iResult;

	SOCKET ConnectSocket = INVALID_SOCKET;
	struct sockaddr_in clientService;

	iResult = WSAStartup( MAKEWORD( 2, 2 ), &wsaData );
	if ( iResult != NO_ERROR )
	{
		fprintf( stderr, "WSAStartup failed: %d\n", iResult );
		return nullptr;
	}

	ConnectSocket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if ( ConnectSocket == INVALID_SOCKET )
	{
		fprintf( stderr, "Error at socket(): %ld\n", WSAGetLastError() );
		WSACleanup();
		return nullptr;
	}

	clientService.sin_family = AF_INET;
	clientService.sin_addr.s_addr = inet_addr( host.c_str() );
	clientService.sin_port = htons( port );

	iResult = connect( ConnectSocket, (SOCKADDR*) &clientService, sizeof( clientService ) );
	if ( iResult == SOCKET_ERROR )
	{
		closesocket( ConnectSocket );
		fprintf( stderr, "Unable to connect to server: %ld\n", WSAGetLastError() );
		WSACleanup();
		return nullptr;
	}

	return new WindowsSocket( ConnectSocket );
}

#endif // ADK_WIN

/*   Winsock2 Channel Impl Ends   */

/*   StdIO Channel Impl Begins   */

class StdIO : public Channel
{
public:
	~StdIO() override;
	bool send( char* msg, size_t len ) override;
	bool recv( char* buf, size_t maxlen ) override;
};

inline StdIO::~StdIO() = default;

inline bool StdIO::send( char* msg, size_t len )
{
	for ( size_t i = 0; i < len; i++ )
	{
		putchar( msg[i] );
	}
	fflush( stdout );
	return true;
}

inline bool StdIO::recv( char* buf, size_t len )
{
	size_t cnt = 0;
	while ( cnt < len )
	{
		int c = getchar();
		if ( c == EOF )
		{
			return false;
		}
		buf[cnt++] = (char) c;
	}
	return true;
}

inline Channel* stdio_channel() { return new StdIO(); }

/*   StdIO Channel Impl Ends   */

/*   TwoDimArray Begins   */

template <typename T>
class RowAccessor
{
public:
	RowAccessor( size_t row_idx, T* data ) : row_idx( row_idx ), data( data ) {}
	T& operator[]( size_t col ) { return data[row_idx + col]; }
	const T& operator[]( size_t col ) const { return data[row_idx + col]; }

private:
	size_t row_idx;
	T* data;
};

template <typename T>
class TwoDimArray
{
public:
	TwoDimArray( size_t length, size_t width, T init_val )
		: length( length ), width( width ), data( new T[length * width] )
	{
		std::fill( data, data + length * width, init_val );
	}
	~TwoDimArray() { delete[] data; }
	TwoDimArray( const TwoDimArray<T>& other )
		: length( other.length ), width( other.width ), data( new T[length * width] )
	{
		std::copy( other.data, other.data + ( length * width ), data );
	}
	TwoDimArray( TwoDimArray&& other ) = delete;

	RowAccessor<T> operator[]( size_t row ) { return RowAccessor<T>( row * length, data ); }
	const RowAccessor<T> operator[]( size_t row ) const { return RowAccessor<T>( row * length, data ); }

private:
	size_t length, width;
	T* data;
};

/*   TwoDimArray Ends   */

/*   Game Logic Begins   */

struct Item
{
	int x, y, id, time, type, param;
	bool eaten, expired;
};

const Item NOT_A_ITEM { 0, 0, -1, 0, 0, 0, false, false };

struct Coord
{
	int x, y;
};

inline bool operator==( const Coord& lhs, const Coord& rhs ) { return ( lhs.x == rhs.x ) && ( lhs.y == rhs.y ); }

inline bool operator!=( const Coord& lhs, const Coord& rhs ) { return !( lhs == rhs ); }

struct Snake
{
	std::vector<Coord> coord_list;
	int id, length_bank, camp;
	Item railgun_item;

	size_t length() const { return coord_list.size(); }

	const Coord& operator[]( size_t idx ) const { return coord_list[idx]; }
};

inline bool operator==( const Snake& lhs, const Snake& rhs ) { return lhs.id == rhs.id; }

inline bool operator!=( const Snake& lhs, const Snake& rhs ) { return lhs.id != rhs.id; }

struct Operation
{
	int type;
};

const Operation OP_RIGHT { 1 };
const Operation OP_UP { 2 };
const Operation OP_LEFT { 3 };
const Operation OP_DOWN { 4 };
const Operation OP_RAILGUN { 5 };
const Operation OP_SPLIT { 6 };

#define PROPERTY( TYPE_NAME, NAME )                   \
public:                                               \
	const TYPE_NAME& NAME() const { return _##NAME; } \
                                                      \
private:                                              \
	TYPE_NAME _##NAME

// Game setting
const static int GROWING_ROUNDS = 8;
const static int ITEM_EXPIRE_LIMIT = 16;
const static int SNAKE_LIMIT = 4;

class Context
{
public:
	friend class SnakeGoAI;

	Context( int length, int width, int max_round, std::vector<Item>&& item_list );
	bool do_operation( const Operation& op );

	const std::vector<Snake>& my_snakes() const;
	std::vector<Snake>& my_snakes();
	const std::vector<int>& tmp_my_snakes() const;
	std::vector<int>& tmp_my_snakes();
	const std::vector<Snake>& opponents_snakes() const;
	std::vector<Snake>& opponents_snakes();

	const Item& find_item( int item_id ) const;
	Item& find_item( int item_id );
	const Snake& find_snake( int snake_id ) const;
	Snake& find_snake( int snake_id );

private:
	PROPERTY( int, length );
	PROPERTY( int, width );
	PROPERTY( int, max_round );
	PROPERTY( int, current_round );
	PROPERTY( int, current_player );

	PROPERTY( TwoDimArray<int>, wall_map );
	PROPERTY( TwoDimArray<int>, snake_map );
	PROPERTY( TwoDimArray<int>, item_map );

	PROPERTY( std::vector<Item>, item_list );

	PROPERTY( std::vector<Snake>, snake_list_0 );
	PROPERTY( std::vector<Snake>, snake_list_1 );
	PROPERTY( std::vector<int>, tmp_list_0 );
	PROPERTY( std::vector<int>, tmp_list_1 );
	int _current_snake_id, _next_snake_id;
	std::vector<int> _new_snakes;
	std::vector<int> _remove_snakes;

	// Helper functions
	Snake& current_snake();
	bool move_snake( const Operation& op );

	void remove_snake( int snake_id );
	void flood_fill( TwoDimArray<int>& map, int x, int y, int v, bool dir_ok[]) const;
	void seal_region();

	bool fire_railgun();
	bool split_snake();
	bool find_next_snake();
	bool round_preprocess();
};

inline Context::Context( int length, int width, int max_round, std::vector<Item>&& item_list )
	: _length( length ), _width( width ), _max_round( max_round ), _current_round( 0 ),
	  _current_player( 1 ), _wall_map { static_cast<size_t>( length ), static_cast<size_t>( width ), -1 },
	  _snake_map { static_cast<size_t>( length ), static_cast<size_t>( width ), -1 },
	  _item_map { static_cast<size_t>( length ), static_cast<size_t>( width ), -1 },
	  _item_list( item_list ), _snake_list_0 {}, _snake_list_1 {}, _tmp_list_0 {}, _tmp_list_1 {},
	  _current_snake_id( 0 ), _next_snake_id( 2 ), _new_snakes {}, _remove_snakes {}
{
	Snake s = { { { 0, width - 1 } }, 0, 0, 0, NOT_A_ITEM };
	_snake_list_0.push_back( s );
	s.coord_list[0] = { length - 1, 0 };
	s.id = s.camp = 1;
	_snake_list_1.push_back( s );

	_snake_map[0][width - 1] = 0;
	_snake_map[length - 1][0] = 1;

	round_preprocess();
}
inline const std::vector<Snake>& Context::my_snakes() const
{
	return _current_player == 0 ? _snake_list_0 : _snake_list_1;
}

inline std::vector<Snake>& Context::my_snakes() { return _current_player == 0 ? _snake_list_0 : _snake_list_1; }

inline const std::vector<int>& Context::tmp_my_snakes() const
{
	return _current_player == 0 ? _tmp_list_0 : _tmp_list_1;
}

inline std::vector<int>& Context::tmp_my_snakes() { return _current_player == 0 ? _tmp_list_0 : _tmp_list_1; }

inline const std::vector<Snake>& Context::opponents_snakes() const
{
	return _current_player == 0 ? _snake_list_1 : _snake_list_0;
}

inline std::vector<Snake>& Context::opponents_snakes() { return _current_player == 0 ? _snake_list_1 : _snake_list_0; }

inline const Item& Context::find_item( int item_id ) const { return _item_list[item_id]; }

inline Item& Context::find_item( int item_id ) { return _item_list[item_id]; }

inline const Snake& Context::find_snake( int snake_id ) const
{
	auto snake = std::find_if( std::begin( _snake_list_0 ), std::end( _snake_list_0 ),
							   [=]( const Snake& other ) { return other.id == snake_id; } );
	if ( snake != std::end( _snake_list_0 ) )
		return *snake;
	return *std::find_if( std::begin( _snake_list_1 ), std::end( _snake_list_1 ),
						  [=]( const Snake& other ) { return other.id == snake_id; } );
}

inline Snake& Context::find_snake( int snake_id )
{
	auto snake = std::find_if( std::begin( _snake_list_0 ), std::end( _snake_list_0 ),
							   [=]( const Snake& other ) { return other.id == snake_id; } );
	if ( snake != std::end( _snake_list_0 ) )
		return *snake;
	return *std::find_if( std::begin( _snake_list_1 ), std::end( _snake_list_1 ),
						  [=]( const Snake& other ) { return other.id == snake_id; } );
}

inline bool Context::do_operation( const Operation& op )
{
	if ( op.type == 5 )
	{
		if ( !fire_railgun() )
			return false;
	}
	else if ( op.type == 6 )
	{
		if ( !split_snake() )
			return false;
	}
	else if ( op.type >= 1 && op.type <= 4 )
	{
		if ( !move_snake( op ) )
			return false;
	}
	else
	{
		return false;
	}

	return !find_next_snake() || round_preprocess();
}

inline Snake& Context::current_snake()
{
	auto& sl = my_snakes();
	return *std::find_if( std::begin( sl ), std::end( sl ),
						  [&]( const Snake& s ) { return s.id == _current_snake_id; } );
}

inline bool Context::move_snake( const Operation& op )
{
	auto& snake = current_snake();
	auto& cl = snake.coord_list;

	if ( _current_round > GROWING_ROUNDS || snake.id != snake.camp )
	{
		if ( snake.length_bank > 0 )
		{
			--snake.length_bank;
		}
		else
		{
			Coord tail = cl.back();
			_snake_map[tail.x][tail.y] = -1;
			cl.pop_back();
		}
	}

	static int dx[] = { 0, 1, 0, -1, 0 }, dy[] = { 0, 0, 1, 0, -1 };
	int nx = cl[0].x + dx[op.type], ny = cl[0].y + dy[op.type];
	Coord nh = { nx, ny };

	bool dead = false, sealed = false;
	if ( nx < 0 || ny < 0 || nx >= _length || ny >= _width || _wall_map[nx][ny] != -1 )
	{
		dead = true;
	}
	else if ( _snake_map[nx][ny] == snake.id )
	{
		if ( cl.size() >= 2 && nh == cl[1] )
		{
			return false;
		}
		cl.insert( cl.begin(), nh );
		sealed = true;
	}
	else if ( _snake_map[nx][ny] != -1 )
	{
		dead = true;
	}

	if ( dead )
	{
		remove_snake( snake.id );
	}
	else if ( sealed )
	{
		seal_region();
	}
	else
	{
		cl.insert( cl.begin(), nh );
		_snake_map[nx][ny] = snake.id;
		if ( _item_map[nx][ny] != -1 )
		{
			auto& item = find_item( _item_map[nx][ny] );
			item.eaten = true;
			if ( item.type == 0 )
			{
				snake.length_bank += item.param;
			}
			else if ( item.type == 2 )
			{
				snake.railgun_item = item;
			}
			else
			{
				return false;
			}
			_item_map[nx][ny] = -1;
		}
	}
	return true;
}

inline void Context::remove_snake( int snake_id )
{
	for ( auto it = _snake_list_0.begin(); it != _snake_list_0.end(); ++it )
	{
		if ( it->id == snake_id )
		{
			for ( auto c : it->coord_list )
			{
				_snake_map[c.x][c.y] = -1;
			}
			_remove_snakes.push_back( snake_id );
			_snake_list_0.erase( it );
			return;
		}
	}
	for ( auto it = _snake_list_1.begin(); it != _snake_list_1.end(); ++it )
	{
		if ( it->id == snake_id )
		{
			for ( auto c : it->coord_list )
			{
				_snake_map[c.x][c.y] = -1;
			}
			_remove_snakes.push_back( snake_id );
			_snake_list_1.erase( it );
			return;
		}
	}
}

inline void Context::flood_fill( TwoDimArray<int>& map, int x, int y, int v, bool dir_ok[]) const
{
	std::queue<Coord> q;
	q.push( { x, y } );
	while ( !q.empty() )
	{
		auto c = q.front();
		q.pop();
		int cx = c.x, cy = c.y;
		if ( cx < 0 || cx >= _length || cy < 0 || cy >= _width)
		{
		    dir_ok[v] = false;
		    continue;
		}
		if (map[cx][cy] != 0)
		    continue;
		map[cx][cy] = v;
		q.push( { cx + 1, cy } );
		q.push( { cx - 1, cy } );
		q.push( { cx, cy + 1 } );
		q.push( { cx, cy - 1 } );
	}
}


inline void Context::seal_region()
{
	static int dx[] = { 1, 0, -1, 0 }, dy[] = { 0, 1, 0, -1 };

	auto& snake = current_snake();
	TwoDimArray<int> grid { (size_t) _length, (size_t) _width, 0 };

	// Mark bounder
	int x0 = snake[0].x, y0 = snake[0].y;
	bool is_head = true;
	int len = 0;
	for ( auto c : snake.coord_list )
	{
		if ( x0 == c.x && y0 == c.y && !is_head )
			break;
		grid[c.x][c.y] = 3;
		is_head = false;
		len++;
	}

	bool dir_ok[] = { false, true, true, true };

	for ( int i = 0; i < len; i++ )
	{
	    // Edge direction
	    int dir1, dir2;
	    int ix = snake[i].x, iy = snake[i].y;
	    int jx = snake[( i + 1 ) % len].x, jy = snake[( i + 1 ) % len].y;
	    if ( ix == jx )
	    {
	        dir1 = iy > jy ? 2 : 0;
	    }
	    else
	    {
	        dir1 = ix > jx ? 1 : 3;
	    }
	    dir2 = ( dir1 + 2 ) % 4;

	    // BFS
	    flood_fill( grid, ix + dx[dir1], iy + dy[dir1], 1, dir_ok);
	    flood_fill( grid, ix + dx[dir2], iy + dy[dir2], 2, dir_ok);
	}

	// Fill wall map
	for ( int i = 0; i < _length; i++ )
	{
		for ( int j = 0; j < _width; j++ )
		{
			if ( dir_ok[grid[i][j]] )
			{
				_wall_map[i][j] = _current_player;

				// Eliminate inner snake
				int snake_id = _snake_map[i][j];
				if ( snake_id != -1 )
				{
					remove_snake( snake_id );
				}
			}
		}
	}
}

inline bool Context::fire_railgun()
{
	auto& snake = current_snake();
	if ( snake.railgun_item.id < 0 )
		return false;
	if ( snake.coord_list.size() < 2 )
		return false;

	const auto& cl = snake.coord_list;
	int cx = cl[0].x, cy = cl[0].y, dx = cl[0].x - cl[1].x, dy = cl[0].y - cl[1].y;
	while ( cx >= 0 && cx < _length && cy >= 0 && cy < _width )
	{
		_wall_map[cx][cy] = -1;
		cx += dx;
		cy += dy;
	}
	snake.railgun_item = NOT_A_ITEM;
	return true;
}

inline bool Context::split_snake()
{
	if ( my_snakes().size() == SNAKE_LIMIT )
		return false;
	auto& snake = current_snake();
	if ( snake.coord_list.size() < 2 )
		return false;

	const auto& cl = snake.coord_list;
	auto mid = ( cl.size() + 1 ) / 2;
	std::vector<Coord> cl_head { cl.begin(), cl.begin() + mid };
	std::vector<Coord> cl_tail { cl.begin() + mid, cl.end() };
	std::reverse( cl_tail.begin(), cl_tail.end() );

	Snake new_snake = { cl_tail, _next_snake_id++, snake.length_bank, snake.camp, NOT_A_ITEM };
	snake.coord_list = cl_head;
	snake.length_bank = 0;
	auto& sl = my_snakes();
	my_snakes().insert(
		std::find_if( std::begin( sl ), std::end( sl ), [&]( const Snake& s ) { return s.id == _current_snake_id; } ) +
			1,
		new_snake );
	_new_snakes.push_back( new_snake.id );
	for ( auto c : cl_tail )
	{
		_snake_map[c.x][c.y] = new_snake.id;
	}

	return true;
}

inline bool Context::find_next_snake()
{
	bool flag = false;
	for ( const auto& s : tmp_my_snakes() )
	{
		if ( s == _current_snake_id )
		{
			flag = true;
			continue;
		}
		if ( flag )
		{
			bool invalid = false;
			for ( int ns : _remove_snakes )
			{
				if ( s == ns )
				{
					invalid = true;
					break;
				}
			}
			if ( invalid )
				continue;
			_current_snake_id = s;
			return false;
		}
	}
	if ( opponents_snakes().empty() )
	{
		if ( !my_snakes().empty() )
			_current_snake_id = my_snakes()[0].id;
	}
	else
	{
		_current_snake_id = opponents_snakes()[0].id;
	}
	return true;
}

inline bool Context::round_preprocess()
{
	_remove_snakes.clear();

	if ( _snake_list_0.empty() && _snake_list_1.empty() )
	{
		return false;
	}

	_new_snakes.clear();

	_current_player = 1 - _current_player;

	if ( _current_player == 0 || ( _current_player == 1 && my_snakes().empty() ) )
	{
		++_current_round;
		if ( _current_round > _max_round )
		{
			return false;
		}

		// remove expired items
		for ( int i = 0; i < _length; ++i )
		{
			auto row = _item_map[i];
			for ( int j = 0; j < _width; ++j )
			{
				int item_id = row[j];
				if ( item_id == -1 )
					continue;
				auto& item = find_item( item_id );
				if ( _current_round >= item.time + ITEM_EXPIRE_LIMIT )
				{
					row[j] = -1;
					item.expired = true;
				}
			}
		}

		// spawn new items
		for ( auto& item : _item_list )
		{
			if ( item.time == _current_round )
			{
				int snake_id = _snake_map[item.x][item.y];
				if ( snake_id == -1 )
				{
					_item_map[item.x][item.y] = item.id;
				}
				else
				{
					item.eaten = true;
					if ( item.type == 0 )
					{
						find_snake( snake_id ).length_bank += item.param;
					}
					else if ( item.type == 2 )
					{
						find_snake( snake_id ).railgun_item = item;
					}
					else
					{
						return false;
					}
				}
			}
		}
	}

	_tmp_list_0.clear();
	for ( const auto& s : _snake_list_0 )
		_tmp_list_0.push_back( s.id );

	_tmp_list_1.clear();
	for ( const auto& s : _snake_list_1 )
		_tmp_list_1.push_back( s.id );

	if ( my_snakes().empty() )
		_current_player = 1 - _current_player;

	return true;
}

/*   Game Logic Ends   */

/*   AI-defined Interface Begins   */

using OpHistory = std::vector<std::vector<Operation>>;

Operation make_your_decision( const Snake& snake_to_operate, const Context& ctx, const OpHistory& op_history );

void game_over( int gameover_type, int winner, int p0_score, int p1_score );

/*   AI-defined Interface Ends   */

/*   AI Controller Begins   */

class SnakeGoAI
{
public:
	SnakeGoAI( int argc, char** argv );
	~SnakeGoAI();

private:
	Channel* ch;
	Context* ctx;
	OpHistory op_history;

	int read_short();
	int read_int();
	std::vector<Item> read_item_list();
	void append_op( Operation op );
	[[noreturn]] void handle_gameover();

	static void crash();
};

inline SnakeGoAI::SnakeGoAI( int argc, char** argv ) : ch( nullptr ), ctx( nullptr ), op_history {}
{
	if ( argc == 1 )
	{
		ch = stdio_channel();
	}
	else if ( argc == 3 )
	{
		ch = socket_channel( argv[1], atoi( argv[2] ) );
	}
	if ( ch == nullptr )
	{
		fprintf( stderr, "Failed to init channel\n" );
		crash();
	}

	int length = read_short(), width = read_short(), max_round = read_int(), player = read_short();
	ctx = new Context( length, width, max_round, std::move( read_item_list() ) );

	while ( true )
	{
		if ( player == ctx->_current_player )
		{
			Operation op = make_your_decision( ctx->current_snake(), *ctx, op_history );
			append_op( op );
			ctx->do_operation( op );
			char msg[] = { 0, 0, 0, 1, (char) op.type };
			bool send_ok = ch->send( msg, 5 );
			int ack_type = read_short();
			if ( ack_type == 0x11 )
			{
				handle_gameover();
			}
			else if ( !send_ok || ack_type != op.type )
			{
				crash();
			}
		}
		else
		{
			int type = read_short();
			if ( type >= 1 && type <= 6 )
			{
				Operation op { type };
				append_op( op );
				ctx->do_operation( op );
			}
			else if ( type == 0x11 )
			{
				handle_gameover();
			}
			else
			{
				crash();
			}
		}
	}
}

inline SnakeGoAI::~SnakeGoAI()
{
	delete ch;
	delete ctx;
}

inline int SnakeGoAI::read_short()
{
	static char c = 0;
	if ( !ch->recv( &c, 1 ) )
		crash();
	return c;
}

#define BIG_ENDIAN_INT( C1, C2 ) \
	( ( ( static_cast<unsigned char>( C1 ) << 8 ) | ( static_cast<unsigned char>( C2 ) ) ) & 0xFFFF )

inline int SnakeGoAI::read_int()
{
	static char c[] = { 0, 0 };
	if ( !ch->recv( c, 2 ) )
		crash();
	return BIG_ENDIAN_INT( c[0], c[1] );
}

inline std::vector<Item> SnakeGoAI::read_item_list()
{
	if ( read_short() != 0x10 )
		crash();

	int item_count = read_int();
	if ( item_count <= 0 )
		crash();
	char* buf = new char[7 * item_count];
	if ( !ch->recv( buf, 7 * item_count ) )
		crash();
	std::vector<Item> item_list { (size_t) item_count };

	for ( int i = 0; i < item_count; i++ )
	{
		auto& item = item_list[i];
		item.x = buf[7 * i];
		item.y = buf[7 * i + 1];
		item.id = i;
		item.type = buf[7 * i + 2];
		item.time = BIG_ENDIAN_INT( buf[7 * i + 3], buf[7 * i + 4] );
		item.param = BIG_ENDIAN_INT( buf[7 * i + 5], buf[7 * i + 6] );
		item.eaten = false;
		item.expired = false;
	}

	return item_list;
}

void SnakeGoAI::append_op( Operation op )
{
	int op_history_idx = 2 * ( ctx->_current_round - 1 ) + ctx->_current_player;
	while ( op_history.size() <= op_history_idx )
	{
		op_history.emplace_back();
	}
	op_history[op_history_idx].push_back( op );
}

inline void SnakeGoAI::handle_gameover()
{
	static char dummy[] = { 0, 0, 0, 1, 1 };
	int gameover_type = read_short(), winner = read_short(), p0_score = read_int(), p1_score = read_int();
	game_over( gameover_type, winner, p0_score, p1_score );

	for ( ;; )
	{
	}
}

inline void SnakeGoAI::crash() { ::exit( -1 ); }

/*   AI Controller Ends   */