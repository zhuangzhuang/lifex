#include <SDL.h>
#include <SDL_gfxPrimitives.h>

#include <tbb/tbb.h>

#include <stdlib.h>

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <string.h>
#include <json/json.h>

#include <windows.h>

const int width = 640;
const int height = 480;
const int ballsize = 4;
const int rows = height / ballsize;
const int cols = width  / ballsize;
const std::string config_name = "config.json";


Uint32 back_color;
Uint32 front_color;
Uint32 line_color;

typedef enum Statue{
	DEAD = 0,
	ALIVE
}Statue;

struct Rule{
	std::string name;
	std::vector<std::vector<Statue> > value;
};

struct Map{
	Statue* status;
	int cols;
	int rows;

	Map(){
		cols = 0;
		rows = 0;
		status = NULL;
	}

	void create(int cols,int rows){
		this->cols = cols;
		this->rows = rows;
		status = (Statue*)malloc(cols*rows*sizeof(Statue));
	}

	void destroy(){
		if(status){
			free(status);
			status = NULL;
		}
	}
};

struct Config{
	std::string rulefile;
	int ruleinit;
	int frontcolor;
	int backcolor;
	unsigned int delay;

	bool parse(const std::string& filename);
};

bool Config::parse(const std::string& filename)
{
	std::ifstream ifs(filename.c_str());
	if(!ifs.is_open()){
		return false;
	}
	
	Json::Value root;
	Json::Reader reader;
	bool parsed = reader.parse(ifs,root);
	if(!parsed){
		return false;
	}
	rulefile   = root.get("rule_file","lexicon.txt").asString();
	ruleinit   = root.get("rule_init",0).asInt();
	frontcolor = root.get("front_color",0x0000000).asInt();
	backcolor  = root.get("back_color",0x0000011).asInt();
	delay      = root.get("delay",0).asUInt();
	return true;
}


void error(char* msg)
{
	std::cerr<<"Error:"<<msg<<"\n";
	system("pause");
	exit(0);
}

void getexe_directory(std::string& curpath)
{
	char name[256];
	GetModuleFileName(NULL,name,256);
	curpath = name;
	curpath = curpath.substr(0,curpath.rfind('\\')+1);
}

std::string string_trim(std::string& str)
{
	if(str.empty()){
		return std::string();
	}
	
	char buf[1024];
	strcpy(buf,str.c_str());
	char *p_begin,*p_end;
	size_t len = str.length();
	p_begin = buf;
	while(isspace(*p_begin)){
		p_begin++;
		if(p_begin > buf+len-1)
			break;
	}
	p_end = buf + len-1;
	while(isspace(*p_end)){
		p_end--;
		if(p_end<buf+1)
			break;		
	}
	return std::string(p_begin,p_end+1);
}

void get_rules(std::vector<Rule>& rules,
			   const std::string &rule_file)
{
	std::ifstream ifs(rule_file.c_str());
	if(!ifs.is_open()){
		error("open cule file failed");
	}
	//parse
	std::vector<std::string> lines;
	std::string line;
	std::string trim_line;
	while(true){
		if(ifs.eof()){
			break;
		}
		std::getline(ifs,line);		
		trim_line = string_trim(line);
		if(trim_line != "")
			lines.push_back(trim_line);
	}
	std::string tmp;
	size_t linelen =  lines.size();
	for(size_t i = 0;i<linelen;i++){
		if(lines[i][0] == '.' || lines[i][0] == '*'){
			Rule rule;
			for(size_t j = i;j>0;j--){
				if(lines[j][0] == ':'){
					tmp = lines[j];
					tmp = tmp.substr(1);
					rule.name = tmp.substr(0,tmp.find(':'));
					break;
				}
			}			
			while(true){
				std::vector<Statue> stat_row;
				for(size_t j = 0;j<lines[i].size();j++){
					if(lines[i][j] == '.'){
						stat_row.push_back(DEAD);
					}else if(lines[i][j] == '*'){
						stat_row.push_back(ALIVE);
					}else{
						error("file format error");
					}
				}
				rule.value.push_back(stat_row);
				i++;
				if(!(lines[i][0] == '.' || lines[i][0] == '*')){
					break;
				}
			}
			rules.push_back(rule);
		}
	}
}


struct Life_eval{
	Life_eval(Map& pre_map,Map& nex_map):
						pre_map(pre_map),
						nex_map(nex_map){
	}

	void operator()(const tbb::blocked_range2d<int,int>& range) const{
		int cols = pre_map.cols;
		int rows = pre_map.rows;

		for(int y = range.cols().begin();y<range.cols().end();y++){
			for(int x = range.rows().begin();x<range.rows().end();x++){
				int value = 0;			
				Statue pre_state = DEAD;
				Statue *p_stat = pre_map.status;
				if(0<x && x<cols - 1 && 0<y && y <rows - 1){
					value = p_stat[(y-1)*cols + x-1] + p_stat[(y-1)*cols + x] +   p_stat[(y-1)*cols + x+1] +
						    p_stat[y*cols + x-1] +                                p_stat[y*cols + x+1] +
							p_stat[(y+1)*cols + x-1] + p_stat[(y+1)*cols + x] + p_stat[(y+1)*cols + x+1];

					pre_state = p_stat[y*cols + x];
					if((pre_state == ALIVE)  && (value == 2) || (value == 3)){
						nex_map.status[y*cols + x] = ALIVE;
					}else{
						nex_map.status[y*cols + x] = DEAD;
					}
				}				
			}
		}
	}
	Map& pre_map;
	Map& nex_map;
};

void draw_ball(SDL_Surface* screen,int row,int col,int ballsize,
					Uint32 color)
{
	int ball_radius = ballsize / 2;
	int x = row * ballsize + ball_radius;
	int y = col * ballsize + ball_radius;
	filledCircleColor(screen,x,y,ball_radius,color);
}

void draw_screen(SDL_Surface* screen,Map& map,SDL_Surface* ball,
						bool show_lines)
{
	SDL_Rect rect;
	rect.x = 0;
	rect.y = 0;
	rect.w = screen->w;
	rect.h = screen->h;

	SDL_FillRect(screen,&rect,back_color);
	
	if(show_lines){
		for(int c = 0;c<cols;c++){
			vlineColor(screen,c*ballsize,0,screen->h,line_color);
		}
		for(int r = 0;r<rows;r++){
			hlineColor(screen,0,screen->w,r*ballsize,line_color);
		}
	}
	for(int y = 0;y<map.rows;y++){
		for(int x = 0;x<map.cols;x++){
			if(map.status[y*map.cols + x] == ALIVE){
				draw_ball(screen,x,y,ballsize,front_color);
			}
		}
	}
}


void change_map(Map& map,Rule& rule,bool& stop)
{
	//Set capture
	SDL_WM_SetCaption(rule.name.c_str(),"");

	//stop calc
	stop = true;

	//Change mapvalue
	int rows = map.rows;
	int cols = map.cols;
	for(int i = 0;i<rows*cols;i++){
		map.status[i] = DEAD;
	}	
	
	int rule_rows = rule.value.size();
	int rule_cols = rule.value[0].size();

	int start_x = (cols - rule_cols)/2;
	int start_y = (rows - rule_rows)/2;
	for( int y = 0;y<rule_rows;y++){
		for(int x = 0;x<rule_cols;x++){
			map.status[(y+start_y)*cols + (x+start_x)] = 
				rule.value[y][x];
		}
	}
}


int main(int argc,char *argv[])
{
	tbb::task_scheduler_init init;
	std::vector<Rule> rules;
	
	std::string curpath;
	getexe_directory(curpath);
	
	std::string config_path = curpath+config_name;		

	Config config;
	if(!config.parse(config_path)){
		error("read config file failed");
	}

	get_rules(rules,curpath+config.rulefile);
	
	size_t rules_size = rules.size();
	int curent_rule = config.ruleinit % rules_size;
	if(rules_size == 0){
		error("no rule");
	}

	SDL_Surface *screen;
	SDL_Surface *ball = NULL;
	
	Uint32 video_flags = SDL_HWPALETTE|SDL_DOUBLEBUF;
	if(SDL_Init(SDL_INIT_VIDEO) <0){
		error("SDL_Init error");
	}
	atexit(SDL_Quit);
	
	screen = SDL_SetVideoMode(width,height,0,video_flags);
	if(screen == NULL){
		error("SDL_SetVideoMode failed");
	}
	
//create color	
	front_color = SDL_MapRGB(screen->format,255,34,225);
	back_color  = SDL_MapRGB(screen->format,23,43,23);
	line_color  = SDL_MapRGB(screen->format,123,143,23);

	bool running = true;
	bool stop = false;
	bool showline = true;

	Map pre_map,nex_map;
	pre_map.create(cols,rows);
	nex_map.create(cols,rows);

//use first map
	change_map(pre_map,rules[curent_rule],stop);

	while(running){
		SDL_Event event;
		while(SDL_PollEvent(&event)){
			switch(event.type){
			case SDL_KEYUP:{
				switch(event.key.keysym.sym){
				case SDLK_SPACE:
					stop = !stop;
					break;
				case SDLK_LEFT:
					curent_rule -- ;					

					curent_rule = curent_rule % rules_size;
					change_map(pre_map,rules[curent_rule],stop);
					break;
				case SDLK_RIGHT:
					curent_rule ++;

					curent_rule = curent_rule % rules_size;
					change_map(pre_map,rules[curent_rule],stop);
					break;
				case SDLK_ESCAPE:
					running = false;
					break;
				case SDLK_l:
					showline = !showline;
					break;
				default:
					break;
				}
			}
			break;
			case SDL_QUIT:
				running = false;
			break;
			}
		}
		
		draw_screen(screen,pre_map,ball,showline);
//calc
		if(!stop){
			for(int i = 0;i<rows*cols;i++){
				nex_map.status[i] = DEAD;
			}
			tbb::parallel_for(tbb::blocked_range2d<int,int>(0,rows,0,cols),
									Life_eval(pre_map,nex_map));
			//std::swap(pre_map.status,nex_map.status);
			Statue *temp;
			temp = pre_map.status;
			pre_map.status = nex_map.status;
			nex_map.status = temp;
		}		

		SDL_Flip(screen);
		SDL_Delay(config.delay);
	}
	pre_map.destroy();
	nex_map.destroy();

	return 0;
}