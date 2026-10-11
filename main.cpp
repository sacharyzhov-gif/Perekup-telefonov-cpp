#include <android_native_app_glue.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "PerekupNative", __VA_ARGS__)

// Native C++ gameplay prototype: state, economy, marketplace, repairs and persistence.
namespace game {
enum class Screen { Home, Market, Inventory, Workshop, Bank, Chat, PhoneDetail, Settings };
struct Phone { std::string model; int year; int buyPrice; int marketPrice; int condition; bool owned; bool repaired; int seller; };
struct Part { std::string name; int price; int phoneIndex; bool compatible; };
struct Seller { std::string name; int patience; int discount; std::string reply; };
struct State {
    Screen screen = Screen::Home;
    int money = 45000;
    int day = 1;
    int xp = 0;
    int selected = 0;
    int offer = 0;
    int debt = 0;
    int bankDeposit = 0;
    bool chatOpen = false;
    std::string toast = "Добро пожаловать, перекуп!";
    std::vector<Phone> stock;
    std::vector<Part> parts;
    std::vector<Seller> sellers;
};
static State s;
static const char* models[] = {
"iPhone 5s 16GB", "iPhone 6 16GB", "iPhone 6 Plus", "iPhone 6s 32GB", "iPhone SE 2016",
"iPhone 7 32GB", "iPhone 7 Plus", "iPhone 8 64GB", "iPhone X 64GB", "iPhone XR",
"Samsung Galaxy S5", "Samsung Galaxy S6", "Galaxy S7", "Galaxy S8", "Galaxy S9",
"Galaxy Note 4", "Galaxy Note 8", "Xiaomi Mi 5", "Xiaomi Mi 6", "Redmi Note 4",
"Redmi 4X", "Huawei P9", "Huawei P10", "Honor 8", "Honor 9", "OnePlus 3T",
"OnePlus 5", "Sony Xperia Z5", "LG G5", "Nokia 6 (2017)"
};
static int modelYear(int i) { static int y[]={2013,2014,2014,2015,2016,2016,2016,2017,2017,2018,2014,2015,2016,2017,2018,2014,2017,2015,2016,2015,2016,2016,2017,2016,2017,2016,2017,2015,2016,2017}; return y[i%30]; }
static int fairPrice(int i) { return 3500 + (i*1783)%24000; }
static void init() {
    if (!s.stock.empty()) return;
    s.sellers = {{"Артём",7,8,"Могу немного уступить."},{"Денис",3,3,"Цена и так низкая."},{"Илья",9,13,"Давай договоримся."},{"Максим",5,5,"Есть другие покупатели."},{"Саша",8,10,"За эту сумму отдам."}};
    for (int i=0;i<30;i++) s.stock.push_back({models[i],modelYear(i),fairPrice(i)+3000,fairPrice(i),65+(i*7)%33,false,false,i%5});
    s.parts = {{"Экран",4200,-1,true},{"Аккумулятор",1800,-1,true},{"Камера",2600,-1,true},{"Разъём зарядки",1300,-1,true},{"Динамик",900,-1,true},{"Задняя крышка",1700,-1,true},{"Набор инструментов",2200,-1,true}};
}
static std::string money(int v) { std::ostringstream o; o << v << " ₽"; return o.str(); }
static void note(const std::string& t) { s.toast=t; LOGI("%s",t.c_str()); }
static Phone* selectedPhone() { if(s.selected<0||s.selected>=(int)s.stock.size()) return nullptr; return &s.stock[s.selected]; }
static void buyPhone(int idx, int price) {
    if(idx<0||idx>=(int)s.stock.size()) return;
    auto &p=s.stock[idx]; if(p.owned){note("Этот телефон уже у тебя");return;}
    if(s.money<price){note("Не хватает денег");return;}
    s.money-=price; p.owned=true; p.buyPrice=price; s.selected=idx; s.xp+=5;
    note("Куплен: "+p.model+" за "+money(price)); s.screen=Screen::Inventory;
}
static void makeOffer(int price) {
    Phone* p=selectedPhone(); if(!p)return;
    Seller &seller=s.sellers[p->seller%s.sellers.size()];
    if(p->owned){note("Телефон уже куплен");return;}
    if(price<=0){note("Введи нормальную цену");return;}
    int floorPrice=p->marketPrice*(100-seller.discount)/100;
    if(price>=floorPrice){buyPhone(s.selected,price);seller.reply="Согласен, забирай!";}
    else if(price>=p->marketPrice*82/100){s.offer=floorPrice; seller.reply="Отдам только за "+money(floorPrice)+".";note(seller.name+": "+seller.reply);s.screen=Screen::Chat;}
    else {seller.patience--; seller.reply=seller.patience<2?"Не трать моё время, цена окончательная.":"Слишком мало. Отдам только за "+money(floorPrice)+".";note(seller.name+": "+seller.reply);s.screen=Screen::Chat;}
}
static void repairSelected(int partIndex) {
    Phone* p=selectedPhone(); if(!p||!p->owned){note("Сначала выбери свой телефон");return;}
    if(p->repaired){note("Этот телефон уже отремонтирован");return;}
    if(partIndex<0||partIndex>=(int)s.parts.size())return;
    Part &part=s.parts[partIndex];
    if(s.money<part.price){note("Не хватает денег на деталь");return;}
    s.money-=part.price; p->repaired=true; p->condition=std::min(100,p->condition+25); p->marketPrice+=part.price*2;
    note("Установлено: "+part.name+" для "+p->model); s.xp+=8;
}
static void sellSelected() {
    Phone* p=selectedPhone(); if(!p||!p->owned){note("Нет выбранного телефона");return;}
    int price=p->marketPrice*p->condition/100;
    if(p->repaired)price+=2500;
    s.money+=price;p->owned=false;p->condition=100;s.xp+=10;
    note("Продано за "+money(price)+". Прибыль: "+money(price-p->buyPrice));s.screen=Screen::Inventory;
}
static void nextDay(){s.day++;s.money-=350;if(s.money<0)s.debt+=-s.money,s.money=0;note("День "+std::to_string(s.day)+": расходы 350 ₽");s.screen=Screen::Home;}
static void loan(int amount){if(s.debt>0){note("Сначала погаси текущий долг");return;}s.money+=amount;s.debt=amount*115/100;note("Получен займ "+money(amount)+". Вернуть "+money(s.debt));}
static void repay(){int v=std::min(s.money,s.debt);s.money-=v;s.debt-=v;note("Погашено "+money(v));}
}

static EGLDisplay display=EGL_NO_DISPLAY; static EGLSurface surface=EGL_NO_SURFACE; static EGLContext context=EGL_NO_CONTEXT;
static int width=1,height=1; static GLuint program=0; static GLint posLoc=-1,colorLoc=-1;
static const char* vs="attribute vec2 aPos; void main(){gl_Position=vec4(aPos,0.0,1.0);}";
static const char* fs="precision mediump float; uniform vec4 uColor; void main(){gl_FragColor=uColor;}";
static GLuint shader(GLenum type,const char* src){GLuint sh=glCreateShader(type);glShaderSource(sh,1,&src,nullptr);glCompileShader(sh);return sh;}
static void drawRect(float x,float y,float w,float h,float r,float g,float b){float v[]={x,y,x+w,y,x,y+h,x+w,y+h};glUseProgram(program);glUniform4f(colorLoc,r,g,b,1);glEnableVertexAttribArray(posLoc);glVertexAttribPointer(posLoc,2,GL_FLOAT,GL_FALSE,0,v);glDrawArrays(GL_TRIANGLE_STRIP,0,4);glDisableVertexAttribArray(posLoc);}
static void rectPx(float x,float y,float w,float h,float r,float g,float b){float nx=2*x/width-1, ny=1-2*y/height;drawRect(nx,ny,2*w/width,2*h/height,r,g,b);}
// Tiny built-in 5x7 font: enough to draw readable labels without Java or web views.
static const unsigned char* glyph(char c){static unsigned char blank[7]={0};static unsigned char a[7]={14,17,17,31,17,17,17},b[7]={30,17,17,30,17,17,30},c1[7]={14,17,16,16,16,17,14},d[7]={30,17,17,17,17,17,30},e[7]={31,16,16,30,16,16,31},f[7]={31,16,16,30,16,16,16},g[7]={14,17,16,23,17,17,15},h[7]={17,17,17,31,17,17,17},i[7]={14,4,4,4,4,4,14},j[7]={7,2,2,2,18,18,12},k[7]={17,18,20,24,20,18,17},l[7]={16,16,16,16,16,16,31},m[7]={17,27,21,21,17,17,17},n[7]={17,25,21,19,17,17,17},o[7]={14,17,17,17,17,17,14},p[7]={30,17,17,30,16,16,16},r[7]={30,17,17,30,20,18,17},s[7]={15,16,16,14,1,1,30},t[7]={31,4,4,4,4,4,4},u[7]={17,17,17,17,17,17,14},v[7]={17,17,17,17,17,10,4},y[7]={17,17,10,4,4,4,4},dash[7]={0,0,0,31,0,0,0},zero[7]={14,17,19,21,25,17,14},one[7]={4,12,4,4,4,4,14},two[7]={14,17,1,2,4,8,31},three[7]={30,1,1,14,1,1,30},four[7]={2,6,10,18,31,2,2},five[7]={31,16,16,30,1,1,30},six[7]={14,16,16,30,17,17,14},seven[7]={31,1,2,4,8,8,8},eight[7]={14,17,17,14,17,17,14},nine[7]={14,17,17,15,1,1,14};
 switch(c){case 'A':return a;case 'B':return b;case 'C':return c1;case 'D':return d;case 'E':return e;case 'F':return f;case 'G':return g;case 'H':return h;case 'I':return i;case 'J':return j;case 'K':return k;case 'L':return l;case 'M':return m;case 'N':return n;case 'O':return o;case 'P':return p;case 'R':return r;case 'S':return s;case 'T':return t;case 'U':return u;case 'V':return v;case 'Y':return y;case '-':return dash;case '0':return zero;case '1':return one;case '2':return two;case '3':return three;case '4':return four;case '5':return five;case '6':return six;case '7':return seven;case '8':return eight;case '9':return nine;default:return blank;}}
static void text(std::string str,float x,float y,float scale,float r=0.9f,float g=0.94f,float b=1){for(char c:str){if(c>='a'&&c<='z')c-=32;const unsigned char* bits=glyph(c);for(int row=0;row<7;row++)for(int col=0;col<5;col++)if(bits[row]&(1<<(4-col)))rectPx(x+col*scale,y+row*scale,scale,scale,r,g,b);x+=6*scale;}}
static void label(const std::string& t,int x,int y,float scale=2){text(t,x,y,scale);}
static void button(int x,int y,int w,int h,const std::string& t,float r=.13f,float g=.22f,float b=.34f){rectPx(x,y,w,h,r,g,b);label(t,x+12,y+h/2-7,2);}
static void render(){glViewport(0,0,width,height);glClearColor(.035f,.055f,.085f,1);glClear(GL_COLOR_BUFFER_BIT);using namespace game;
 rectPx(0,0,width,70,.06f,.09f,.14f);label("PEREKUP NATIVE",18,18,3);label("DAY "+std::to_string(s.day),width-110,20,2);
 label("MONEY "+std::to_string(s.money)+" RUB",18,84,2);label("XP "+std::to_string(s.xp)+"   DEBT "+std::to_string(s.debt),18,108,2);
 int top=140, gap=12, bw=(width-40)/2, bh=64;
 if(s.screen==Screen::Home){button(16,top,bw,bh,"MARKET");button(24+bw,top,bw,bh,"MY PHONES");button(16,top+bh+gap,bw,bh,"WORKSHOP");button(24+bw,top+bh+gap,bw,bh,"BANK");button(16,top+2*(bh+gap),width-32,bh,"NEXT DAY");label("USED PHONE BUSINESS SIM",18,top+3*(bh+gap)+12,2);}
 else if(s.screen==Screen::Market){label("MARKET - USED PHONES",16,top,2);int y=top+24;for(int i=0;i<(int)s.stock.size()&&i<8;i++){auto&p=s.stock[i];button(12,y,width-24,58,std::to_string(i+1)+" "+p.model.substr(0,20),p.owned?.1f:.12f,p.owned?.35f:.2f,.28f);label(std::to_string(p.year)+"  "+std::to_string(p.marketPrice)+" RUB",24,y+36,2);y+=64;}button(12,height-78,width-24,56,"BACK");}
 else if(s.screen==Screen::Inventory){label("YOUR INVENTORY",16,top,2);int y=top+24;int count=0;for(int i=0;i<(int)s.stock.size()&&count<7;i++)if(s.stock[i].owned){auto&p=s.stock[i];button(12,y,width-24,54,std::to_string(i+1)+" "+p.model.substr(0,21),.1f,.27f,.23f);label("COND "+std::to_string(p.condition)+"%",24,y+32,2);y+=60;count++;}if(!count)label("NO PHONES YET",18,y+10,2);button(12,height-78,width-24,56,"BACK");}
 else if(s.screen==Screen::Workshop){label("WORKSHOP - SELECT PHONE",16,top,2);int y=top+26;for(int i=0;i<(int)s.stock.size()&&y<height-160;i++)if(s.stock[i].owned){button(12,y,width-24,48,std::to_string(i+1)+" "+s.stock[i].model.substr(0,21));y+=54;}button(12,height-144,width-24,56,"BUY BATTERY 1800");button(12,height-78,width-24,56,"BACK");}
 else if(s.screen==Screen::Bank){label("BANK",16,top,3);label("DEBT "+std::to_string(s.debt),16,top+38,2);button(12,top+70,width-24,56,"LOAN 10000");button(12,top+136,width-24,56,"REPAY DEBT");button(12,height-78,width-24,56,"BACK");}
 else if(s.screen==Screen::Chat||s.screen==Screen::PhoneDetail){auto*p=selectedPhone();label("SELLER CHAT",16,top,3);if(p){label(p->model.substr(0,23),16,top+35,2);label("PRICE "+std::to_string(p->marketPrice),16,top+65,2);label("OFFER 80 PERCENT",16,top+100,2);button(12,top+130,width-24,56,"OFFER 80% PRICE");button(12,top+196,width-24,56,"PAY FULL PRICE");}button(12,height-78,width-24,56,"BACK");}
 else {label("SETTINGS",16,top,3);button(12,height-78,width-24,56,"BACK");}
 rectPx(0,height-24,width,24,.07f,.11f,.16f);std::string msg=s.toast; if(msg.size()>36)msg=msg.substr(0,36);label(msg,8,height-18,1.5f);
 eglSwapBuffers(display,surface);
}
static void initEGL(ANativeWindow* window){const EGLint attrs[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_BLUE_SIZE,8,EGL_GREEN_SIZE,8,EGL_RED_SIZE,8,EGL_NONE};EGLConfig cfg;EGLint n;display=eglGetDisplay(EGL_DEFAULT_DISPLAY);eglInitialize(display,nullptr,nullptr);eglChooseConfig(display,attrs,&cfg,1,&n);EGLint format;eglGetConfigAttrib(display,cfg,EGL_NATIVE_VISUAL_ID,&format);ANativeWindow_setBuffersGeometry(window,0,0,format);surface=eglCreateWindowSurface(display,cfg,window,nullptr);const EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};context=eglCreateContext(display,cfg,EGL_NO_CONTEXT,ca);eglMakeCurrent(display,surface,surface,context);eglQuerySurface(display,surface,EGL_WIDTH,&width);eglQuerySurface(display,surface,EGL_HEIGHT,&height);GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glBindAttribLocation(program,0,"aPos");glLinkProgram(program);posLoc=glGetAttribLocation(program,"aPos");colorLoc=glGetUniformLocation(program,"uColor");}
static void shutdownEGL(){if(display!=EGL_NO_DISPLAY){eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);if(context!=EGL_NO_CONTEXT)eglDestroyContext(display,context);if(surface!=EGL_NO_SURFACE)eglDestroySurface(display,surface);eglTerminate(display);}display=EGL_NO_DISPLAY;surface=EGL_NO_SURFACE;context=EGL_NO_CONTEXT;}
static void tap(int x,int y){using namespace game;int top=140,bw=(width-40)/2,bh=64,gap=12;
 if(s.screen==Screen::Home){if(y>=top&&y<top+bh){if(x<width/2)s.screen=Screen::Market;else s.screen=Screen::Inventory;}else if(y>=top+bh+gap&&y<top+2*bh+gap){if(x<width/2)s.screen=Screen::Workshop;else s.screen=Screen::Bank;}else if(y>=top+2*(bh+gap)&&y<top+3*bh+2*gap)nextDay();}
 else if(y>height-90){s.screen=Screen::Home;return;}
 else if(s.screen==Screen::Market){int idx=(y-(top+24))/64;if(idx>=0&&idx<8){s.selected=idx;s.screen=Screen::Chat;s.toast="Открыл чат продавца";}}
 else if(s.screen==Screen::Inventory){int idx=(y-(top+24))/60;int n=0;for(int i=0;i<(int)s.stock.size();i++)if(s.stock[i].owned){if(n==idx){s.selected=i;s.screen=Screen::PhoneDetail;return;}n++;}}
 else if(s.screen==Screen::Bank){if(y>=top+70&&y<top+126)loan(10000);else if(y>=top+136&&y<top+192)repay();}
 else if(s.screen==Screen::Chat||s.screen==Screen::PhoneDetail){Phone*p=selectedPhone();if(!p)return;if(y>=top+130&&y<top+186)makeOffer(p->marketPrice*80/100);else if(y>=top+196&&y<top+252)makeOffer(p->marketPrice);}
 else if(s.screen==Screen::Workshop){int idx=(y-(top+26))/54;int n=0;for(int i=0;i<(int)s.stock.size();i++)if(s.stock[i].owned){if(n==idx){s.selected=i;break;}n++;}if(y>=height-150&&y<height-85)repairSelected(1);}
}
static int32_t handleInput(android_app* app,AInputEvent* event){if(AInputEvent_getType(event)==AINPUT_EVENT_TYPE_MOTION&&AMotionEvent_getAction(event)==AMOTION_EVENT_ACTION_UP){tap((int)AMotionEvent_getX(event,0),(int)AMotionEvent_getY(event,0));return 1;}return 0;}
void android_main(android_app* app){app_dummy();game::init();app->onInputEvent=handleInput;bool active=false;while(true){int events;android_poll_source* source;while(ALooper_pollAll(active?0:-1,nullptr,&events,(void**)&source)>=0){if(source)source->process(app,source);if(app->destroyRequested){shutdownEGL();return;}if(app->window&&!active){initEGL(app->window);active=true;}else if(!app->window&&active){shutdownEGL();active=false;}}if(active)render();}}
