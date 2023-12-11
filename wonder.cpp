#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <fstream>
#include <tchar.h>
#include <vector>
#include <cmath>
#include <iomanip>
#include <chrono>
#include <ctime>   
#include <stdio.h>
#include <cstdlib>
#include <errhandlingapi.h>
#include <map>
#include <set>
#include <string>
#define cerr(x) do { std::cerr << std::hex << #x << ": " << x << std::endl; } while (0)
using std::cout;
using std::cin;
using std::abs;
using std::string;
typedef unsigned long long ull;
const long double toDeg = 57.295779513082320876798154814105;
const ull baseAddress = 0x09a15ef0;
ull r13, last_r13, addrVel, addrInternal, addrPos, addrFace, addrIgt, addrPowerup, tull;
DWORD pID, last_pID;
HANDLE processHandle = NULL;    
short shift = 0;
const char* emuTitle = "yuzu | tas-perfect-mainline-mainline-636-13044-gcc7d0fa41 | Super Mario Bros. Wonder (64-bit) | 1.0.0 | NVIDIA";
//const char* emuTitle = "yuzu | tas-perfect-mainline-636-13112-g0e1df12b5 | Super Mario Bros. Wonder (64-bit) | 1.0.0 | NVIDIA";
bool global = 1, lastauto = 0, automode = 1, display = 0, focused = 0, realtoggle = 0, firstboot = 0;
const unsigned char keyonce = 'N', keyhold = 'H', keyglobal = 'G', keyreset = 'R', keyauto = 'A', keytp = 'P', keyturn = 'O', keyvel = 'I', keycommand = VK_OEM_2, keyexport = '1', keydisplay = 'D', keyreal = 'L';
char tempc, buf[128], consoleTitle[600], foregroundTitle[600];
static char savestate[0xc06c], savestate2[0x113d14];
string command, line, word;
float framecount = 0, internal, velAng, faceAng, hvel, yvel, svel, realHvel, realVelAng, frontvel, sidevel, eff, effy, fine = 1, igtImproper;
float avgVelLog[200] = {}, avgVelSum = 0;
int igtFrames, tempi;

struct V3f{
	float x;
	float y;
	float z;
	float hvel(){
		return sqrt(x*x+y*y);
	}
	float svel(){
		return sqrt(x*x+y*y+z*z);
	}
	float ang(){
		return fmod(360+toDeg*atan2(y, x), 360);
	}
	float nz(){
		return -z;
	}
	void conjugate(){
		z = -z;
	}
	V3f normalize2d(){
		float b = 1 / this->hvel();
		return V3f{this->x*b, this->y*b, this->z*b};
	}
	operator bool(){
		return bool(x) | bool(y) | bool(z);
	}
};

V3f operator+(V3f a, V3f b){
	return V3f{a.x+b.x, a.y+b.y, a.z+b.z};
}
V3f operator-(V3f a, V3f b){
	return V3f{a.x-b.x, a.y-b.y, a.z-b.z};
}
V3f operator*(V3f a, float k){
	return V3f{a.x*k, a.y*k, a.z*k};
}
V3f operator*(V3f a, V3f b){
	return V3f{a.x*b.y, a.y*b.z, a.z*b.x};
}
bool operator==(V3f a, V3f b){
	return a.x == b.x && a.y == b.y && a.z == b.z;
}
std::ostream& operator<<(std::ostream& os, V3f a){
	return os << a.x << " " << a.y << " " << a.z;
}


V3f vel, pos, face, lastVel, lastPos, realVel,  savedVel, savedPos, savedFace, accel;

/*char* dataArr[5] = {(char*)&last_pID, (char*)&last_r13, (char*)&savedVel, (char*)&savedPos, (char*)&savedFace};
size_t dataArrSize[5] = {sizeof(last_pID), 8, 12, 12, 12};*/

struct ptrsize{
	char* p;
	std::size_t z;
};

std::vector<ptrsize> saveDataArr;
template<typename T> void ptrsizePush(std::vector<ptrsize> *v, T *p){
	(*v).push_back(ptrsize{(char*)p, sizeof(*p)});
}

const int printColumnCount = 18;
bool printColumnToggle[printColumnCount] = {1,1,1,1,1,1,1,0,0,0,0,1,1,1,1,1,1,1};
std::vector<float*> printColumnSource;
std::vector<std::string> printColumnFormat;

void printColumnSetup(){
	printColumnSource.clear();					printColumnFormat.clear();
	printColumnSource.push_back(&igtImproper);	printColumnFormat.push_back("%-7.2f");
	printColumnSource.push_back(&vel.x);		printColumnFormat.push_back("vx:%-7.4g");
	printColumnSource.push_back(&vel.y);		printColumnFormat.push_back("y:%-7.4g");
	printColumnSource.push_back(&vel.z);		printColumnFormat.push_back("z:%-7.4g");
	printColumnSource.push_back(&hvel);			printColumnFormat.push_back("h:%-8g");
	printColumnSource.push_back(&internal);		printColumnFormat.push_back("i:%-8g");
	printColumnSource.push_back(&sidevel);		printColumnFormat.push_back("si:%-7.4g");
	printColumnSource.push_back(&svel);			printColumnFormat.push_back("xyz:%-8g");
	printColumnSource.push_back(&realVel.x);	printColumnFormat.push_back("rx:%-7.4g");
	printColumnSource.push_back(&realVel.y);	printColumnFormat.push_back("y:%-7.4g");
	printColumnSource.push_back(&realVel.z);	printColumnFormat.push_back("z:%-7.4g");
	printColumnSource.push_back(&velAng);		printColumnFormat.push_back("a:%-8.3f");
	printColumnSource.push_back(&faceAng);		printColumnFormat.push_back("f:%-8.3f");
	printColumnSource.push_back(&pos.x);		printColumnFormat.push_back("px:%-9g");
	printColumnSource.push_back(&pos.y);		printColumnFormat.push_back("y:%-9g");
	printColumnSource.push_back(&pos.z);		printColumnFormat.push_back("z:%-9g");
	printColumnSource.push_back(&eff);			printColumnFormat.push_back("e:%-5.3g");
}

void printColumnPrint(){
	int i=0, j=0;
	for(auto it = printColumnSource.begin(); it != printColumnSource.end(); i++, it++){
		if(printColumnToggle[i]){
			cout<<(j++? "": "\n");
			printf(printColumnFormat[i].c_str(), *printColumnSource[i]);
		}
	}
}

uintptr_t GetModuleBaseAddress(TCHAR* lpszModuleName, DWORD pID) {
    uintptr_t dwModuleBaseAddress = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pID); // make snapshot of all modules within process
    MODULEENTRY32 ModuleEntry32 = { 0 };
    ModuleEntry32.dwSize = sizeof(MODULEENTRY32);
 
    if (Module32First(hSnapshot, &ModuleEntry32)) //store first Module in ModuleEntry32
    {
        do {
            if (_tcscmp(ModuleEntry32.szModule, lpszModuleName) == 0) // if Found Module matches Module we look for -> done!
            {
                dwModuleBaseAddress = (uintptr_t)ModuleEntry32.modBaseAddr;
                break;
            }
        } while (Module32Next(hSnapshot, &ModuleEntry32)); // go through Module entries in Snapshot and store in ModuleEntry32
 
 
    }
    CloseHandle(hSnapshot);
    return dwModuleBaseAddress;
}

uintptr_t GetR13(DWORD pID){
	const int collectj = 1, collectk = 16;
    const uintptr_t r13min = 0x10000000000, r13max = 0x40000000000;
    std::multiset<uintptr_t> r13total;
    std::set<uintptr_t> r13multiinsnap;
    std::multiset<uintptr_t> r13multiinsnaptemp;
    std::set<uintptr_t> r13finalsel;
    CONTEXT ctx;
    LPCONTEXT pctx = &ctx;
    ctx.ContextFlags = CONTEXT_FULL;
    for(int k=0; k<collectk; k++){
    std::vector<DWORD> threadList;
    HANDLE tempThreadHandle = NULL;
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (h != INVALID_HANDLE_VALUE) {
		THREADENTRY32 te;
		te.dwSize = sizeof(te);
		if (Thread32First(h, &te)) {
			do {
				if (te.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(te.th32OwnerProcessID)) {
					if(te.th32OwnerProcessID == pID){
    					threadList.push_back(te.th32ThreadID);
						//printf("Process 0x%04x Thread 0x%04x\n", te.th32OwnerProcessID, te.th32ThreadID);
					}
				}
				te.dwSize = sizeof(te);
			} while (Thread32Next(h, &te));
		}
		CloseHandle(h);
	}
	/*std::cout<<threadList.size()<<"\n";
	for(int i=0; i<threadList.size(); i++){
		std::cout<<GetThreadId(threadList[i])<<"\n";
	}*/
    for(int j=0; j<collectj; j++){
    for(int i=0; i<threadList.size(); i++){
    	HANDLE temp = OpenThread(THREAD_ALL_ACCESS, FALSE, threadList[i]);
		SuspendThread(temp);
		GetThreadContext(temp, pctx);
		ResumeThread(temp);
		CloseHandle(temp);
		uintptr_t r13t = ctx.R13;
		if(r13t >= r13min && r13t < r13max){
			//std::cout<<"Thread "<<threadList[i]<<": "<<std::hex<<ctx.R13<<"\n";
			r13total.insert(r13t);
			r13multiinsnaptemp.insert(r13t);
		}
	}
	for(auto it = r13multiinsnaptemp.begin(); it != r13multiinsnaptemp.end(); ++it){
		//std::cout<<*it<<"\n";
		//std::cout<<r13multiinsnaptemp.count(*it)<<"\n";
		if(r13multiinsnaptemp.count(*it) > 1){
			r13multiinsnap.insert(*it);
		}
	}
	r13multiinsnaptemp.clear();
	}
	}
	/*for(auto it = r13multiinsnap.begin(); it != r13multiinsnap.end(); ++it){
		std::cout<<*it<<"\n";
	}*/
	for(auto it = r13total.begin(), it2 = r13total.end(); it != it2; ++it){
		if(r13total.count(*it) != collectj*collectk && r13total.count(*it) > 1){
			//std::cout<<std::hex<<*it<<"\n";
			//std::cout<<r13total.count(*it)<<"\n";
			r13finalsel.insert(*it);
			r13total.erase(*it);
		}
		//it2 = r13total.end();
	}
	if(r13finalsel.size() == 1){
		//std::cout<<std::hex<<*r13finalsel.begin()<<"\n";
		return *r13finalsel.begin();
	}
	for(auto it = r13finalsel.begin(); it != r13finalsel.end(); ++it){
		if(r13multiinsnap.count(*it)){
			return *it;
		}
	}
	puts("failed, trying again...");
	return GetR13(pID);
}

char key[256] = {};
void PollInput(){
	for(unsigned char i=0; i<255; i++){
		short j = GetAsyncKeyState(i);
		key[i] = ((key[i]^1)&j)*2+j;
	}
	return;
}
void toclip(std::string s){
	s += " ";
	OpenClipboard(0);
	EmptyClipboard();	
	HGLOBAL hg=GlobalAlloc(GMEM_MOVEABLE,s.size());
	if (!hg){
		CloseClipboard();
		cout << "Failed to copy " << s << " to clipboard...\n";
		return;
	}
	memcpy(GlobalLock(hg),s.c_str(),s.size());
	GlobalUnlock(hg);
	SetClipboardData(CF_TEXT,hg);
	CloseClipboard();
	GlobalFree(hg);
	cout << "Copied " << s << " to clipboard!\n";
}

long double goodRound(long double x, long double toNearest){
	return round(x / toNearest) * toNearest;
}

template<typename T>
void readMem(ull address, T *location){
	ReadProcessMemory(processHandle, (LPVOID)(address), location, sizeof(*location), NULL);
}

template<typename T>
void writeMem(ull address, T *location){
	WriteProcessMemory(processHandle, (LPVOID)(address), location, sizeof(*location), 0);
}

const std::vector<ull> pointOffsets[4]{{baseAddress, 0x58, 0x2c}, //vel
{baseAddress, 0x1c8, 0x150, 0xa0}, //internal
{baseAddress, 0x238, 0x68, 0x0},//pos
{0x20fda94fb0, 0x8, 0x3a0, 0x138, 0x38}};//igt

ull calOffset(const int index){
	ull address = 0;
	auto *p = &pointOffsets[index];
	for(auto it = (*p).begin(), en = (*p).end()  -1; it != en; ++it){
		readMem(address+r13+(*it), &address);
	}
	return address+r13+(*p).back();
}

void calOffsets(){
	//addrVel = calOffset(0);
	/*addrInternal = calOffset(1);
	addrPos = calOffset(2);
	addrVel = addrPos + 0x24;
	addrFace = addrPos + 0xc;
	addrIgt = calOffset(3);
	addrPowerup = addrPos + 0xc0;*/
	readMem(tull + 0xc8, &addrPos);
	addrPos += r13 + 0x2b4;
	addrVel = addrPos + 0x6c;
}

template<typename T>
void saveload(T *active, T *saved){
	if(shift){
		*saved = *active;
		puts("saved!");
	}else{
		*active = *saved;
		puts("loaded!");
	}
}

int readFile(){
	return 0;
	bool readFail = 0;
	int readFailCount = 4;
	do{
		readFail = 0;
		std::ifstream rf("3dwram.dat", std::ios::out | std::ios::binary);
		if(!rf) {
			//puts("Failed to open read!");
			//readFail = 1;
			firstboot = 1;
			return 0;
		}else{
			/*for(int i=0; i<sizeof(dataArr)/8; i++){
				rf.read((char*)dataArr[i], dataArrSize[i]);
			}*/
			for(auto it = saveDataArr.begin(); it != saveDataArr.end(); ++it){
				rf.read((*it).p, (*it).z);
			}
			string s;
			printColumnFormat.clear();
			for(auto it = printColumnSource.begin(); it != printColumnSource.end(); ++it){
				string s;
				rf >> s;
				printColumnFormat.push_back(s);
			}
			rf.close();
			if(!rf.good()){
				puts("Failed to read file!");
			readFail = 1;
			}
		}
	}while(readFail && --readFailCount);
	if(!readFailCount){
		//puts("Save data uninitialized!");
		if(remove("3dwram.dat")){
			puts("\"3dwram.dat\" corrupted. Please manually delete file, then press enter.");
			getline(cin, command);
		}
		return -1;
	}
}

void writeFile(){
	return;
	bool writeFail = 0;
	//do{
		writeFail = 0;
		std::ofstream wf("3dwram.dat", std::ios::out | std::ios::binary);
		if(!wf) {
			puts("Failed to open write!");
			writeFail = 1;
		}else{
			/*for(int i=0; i<sizeof(dataArr)/8; i++){
				wf.write((char*)dataArr[i], dataArrSize[i]);
			}*/
			for(auto it = saveDataArr.begin(); it != saveDataArr.end(); ++it){
				wf.write((*it).p, (*it).z);
			}
			for(auto it = printColumnFormat.begin(); it != printColumnFormat.end(); ++it){
				wf << *it << " ";
			}
			wf.close();
			if(!wf.good()){
				puts("Failed to write file!");
			writeFail = 1;
			}
		}
	//}while(writeFail);
}

ull GetPlayerChannel(){
	puts("searching...");
	ull ret = r13 + 0x1d00000000;
	ull scale = 0x1000;
	ull scale2 = 0x800000000 / scale;
	void* c = malloc(scale2);
	void* ccopy = c;
	for(ull i=0; i<0x800000000; i+=scale2){
		c = ccopy;
		ReadProcessMemory(processHandle, (LPVOID)(ret+i), (void*)((ull)c), scale2, NULL);
		for(ull j=0; j<scale2; j+=8){
			if(*(unsigned int *)c == 0x82948850){
				free(ccopy);
				return ret+i;
			}
			c = (void*)((ull)c+8);
			ret += 8;
		}
	}
	/*while(1){
		c = (void*)((ull)c+8);
		ret += 8;
		if(*(unsigned int *)c == 0x82948850){
			free(ccopy);
			return ret;
		}
		/*if((ull)c % 0x1000000 == 0){
			cerr(/**(unsigned int *)c);
		}
	}*/
}

void exiting(){
	last_pID = pID;
	last_r13 = r13;
	writeFile();
}

void createData(){
	
}
 
int main() {
	labelHardReset:
	puts("initializing...");
 
    HWND hGameWindow = FindWindow(NULL, emuTitle);
    if (hGameWindow == NULL) {
        std::cout << "Failed to find window" << std::endl;
        return 0;
    }
    GetWindowThreadProcessId(hGameWindow, &pID);
    processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pID);
    if (processHandle == INVALID_HANDLE_VALUE || processHandle == NULL) { // error handling
        std::cout << "Failed to open process" << std::endl;
        return 0;
    }
	saveDataArr.clear();
	ptrsizePush(&saveDataArr, &last_pID);
	ptrsizePush(&saveDataArr, &last_r13);
	ptrsizePush(&saveDataArr, &savedVel);
	ptrsizePush(&saveDataArr, &savedPos);
	ptrsizePush(&saveDataArr, &savedFace);
	ptrsizePush(&saveDataArr, &printColumnToggle);
	printColumnSetup();
	if(readFile() == -1){
		goto labelHardReset;
	}
	r13 = (pID != last_pID || shift)? GetR13(pID) : last_r13;
	cerr(r13);
	puts("50889482");
	//std::cin>>std::hex>>tull;
	tull = GetPlayerChannel();
	cerr(tull);
	calOffsets();
	cerr(addrPos);
	cerr(addrVel);
	std::atexit(exiting);
    puts("done!");
	while(true) {
		Sleep(0.5);
		if(1){
			Sleep(3);
			readMem(addrVel, &vel);
			//readMem(addrInternal, &internal);
			readMem(addrPos, &pos);
			//readMem(addrFace, &face);
			//readMem(addrIgt, &igtFrames);
			realVel = (pos - lastPos);
			realVel = V3f{realVel.x*60, realVel.y*60, realVel.z*60};
			accel = vel - lastVel;
			eff = realVel.x / vel.x;
			effy = realVel.y / vel.y;
			//realHvel = realVel.hvel();
	    	if(!realVel/* && !key[keyonce]*/){
	    		;
			}else{
				//eff = realHvel/lastVel.hvel();//hvel of prev frame
				//hvel = vel.hvel();
				//yvel = realtoggle? realVel.y : vel.y;
				//frontvel = (abs(hvel - lastVel.hvel()) > 1.1)? realHvel : internal;
				//sidevel = sqrt(hvel * hvel - internal * internal);
				//velAng = realtoggle? realVel.ang() : vel.ang();
				//faceAng = face.ang();
				//hvel = realtoggle? realVel.hvel() : hvel;
				//svel = (realtoggle? realVel : vel).svel();
				//avgVelSum += hvel - avgVelLog[int(framecount)%200];
				//avgVelLog[int(framecount)%200] = hvel;
				//igtImproper = 1+igtFrames/44+igtFrames%44/100.0;
				//vel.conjugate();
				//pos.conjugate();
				//realVel.conjugate();
				//printColumnPrint();
		    	/*if(display){
					printf("h: %.3g\ty: %.3g\t avg: %.3g\n", hvel, yvel, avgVelSum/200);
				}else{
					printf("%d y:%.4g\th:%g\tin:%g\tsi:%.4g\ta:%.5f\tf:%g\tpos x:%g\ty:%g\tz:%g\te:%.3g\n", framecount, yvel, hvel, internal, sidevel, velAng, faceAng, pos.x, pos.y, (-pos.z), eff);
				}*/
				printf("%.0f\tvel x:%.4f\ty:%.4f\tacc x:%.4f\ty:%.4f\tpos x:%.4f\ty:%.4f\te x:%.3g\ty:%.3g\n", framecount, vel.x, vel.y, accel.x, accel.y, pos.x, pos.y, eff, effy);
				//vel.conjugate();
				//pos.conjugate();
				//realVel.conjugate();
				//*sn*/printf(/*buf2, sizeof(buf2), */"%d x:%.5f\ty:%.5f\tz:%.5f\th:%.5f\tin:%.5f\ta:%.5f\tf:%.6f\tpos x:%g\ty:%g\tz:%g\n", framecount, vel[0], vel[1], (-vel[2]), hvel, interspd, ang, face, pos[0], pos[1], (-pos[2]));
				framecount += 1;
				lastVel = vel;
				lastPos = pos;
			}
		}
		if(1/*!(rand()&7)*/){
			calOffsets();
			GetWindowTextA(GetForegroundWindow(), foregroundTitle, 600);
			GetConsoleTitle(consoleTitle, 600);
			focused = !strcmp(consoleTitle, foregroundTitle) || !strcmp(emuTitle, foregroundTitle)/* || global*/;
		}
		//continue;
		//std::cout<<!strcmp(consoleTitle, foregroundTitle)<<global<<((!strcmp(consoleTitle, foregroundTitle)) || global)<<"\n";
    	if(focused){
			PollInput();
			shift = GetAsyncKeyState(VK_SHIFT);
	    	if(shift){
				fine = 100;
			}else{
				fine = 1;
			}
			for(unsigned char i=0; i<255; i++){
				if(key[i]&2){
					switch (i){
//						case keydisplay:
//							display = 1-display;
//							cout<<"Changed display profile to "<<display<<"!\n";
//							break;
//						case keyreal:
//							realtoggle = 1-realtoggle;
//							cout<<"Real velocity "<<(realtoggle?"en":"dis")<<"abled!\n";
//							if(shift){
//								printColumnToggle[1] = 1-realtoggle;
//								printColumnToggle[2] = 1-realtoggle;
//								printColumnToggle[3] = 1-realtoggle;
//								printColumnToggle[6] = realtoggle;
//								printColumnToggle[7] = realtoggle;
//								printColumnToggle[8] = realtoggle;
//							}
//							break;
//						case keyvel:
//							saveload(&vel, &savedVel);
//							writeMem(addrVel, &vel);
//							break;
//						case keyturn:
//							saveload(&face, &savedFace);
//							writeMem(addrFace, &face);
//							break;
						case keytp:
							saveload(&pos, &savedPos);
							writeMem(addrPos, &pos);
							break;
						case keyreset:
							if(shift){
								goto labelHardReset;
							}else{
								framecount = 0;
								calOffsets();
							}
							break;
						case keyglobal:
							/*global = 1-global;
							cout<<"Global input "<<(global?"en":"dis")<<"abled!\n";*/
							break;
						case VK_LEFT:
							pos.x-=0.01*fine;
							writeMem(addrPos, &pos);
							break;
						case VK_UP:
							pos.z-=0.01*fine;
							writeMem(addrPos, &pos);
							break;
						case VK_RIGHT:
							pos.x+=0.01*fine;
							writeMem(addrPos, &pos);
							break;
						case VK_DOWN:
							pos.z+=0.01*fine;
							writeMem(addrPos, &pos);
							break;
//						case VK_OEM_4:
//							faceAng+=0.01*fine;
//							face.x = cos(faceAng/toDeg);
//							face.z = sin(faceAng/-toDeg);
//							writeMem(addrFace, &face);
//							break;
//						case VK_OEM_6:
//							faceAng-=0.01*fine;
//							face.x = cos(faceAng/toDeg);
//							face.z = sin(faceAng/-toDeg);
//							writeMem(addrFace, &face);
//							break;
						case keyexport:
							snprintf(buf, sizeof(buf), "%g\t%g", pos.x, pos.y);
							toclip(buf);
							break;
						case keycommand:
							break;
							//cin.clear();
							//cin.ignore(INT_MAX, '/');
							cout<<"Set ";
							//Sleep(16);
							//cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
							//while ((getchar()) != '/');
							getline(cin, line);
							for(int i=0; i<line.size(); i++){
								if(line[i] == '/'){
									line = line.substr(i+1, line.size()-i-1);
									i = -1;
								}
							}
							//cout<<line;
							//cin>>tempc>>tempf;
							/*switch(tempc){
								case 'p':
									//tempi = tempf;
									WriteProcessMemory(processHandle, (LPVOID)(addrrealpos+0xc0), &tempi, sizeof(tempi), 0);
									WriteProcessMemory(processHandle, (LPVOID)(addrrealpos+0xc4), &tempi, sizeof(tempi), 0);
									break;
								default:
									pos[2] = -pos[2];
									//pos[tempc-'x'] = tempf;
									pos[2] = -pos[2];
									WriteProcessMemory(processHandle, (LPVOID)(addrrealpos), &pos, sizeof(pos), 0);
									break;
							}*/{
							int i = stoi(line);
							
							printColumnToggle[i] = 1-printColumnToggle[i];}
							break;
						//case '1':
							//ReadProcessMemory(processHandle, (LPVOID)(addrrealpos-0x2c), &savestate, sizeof(savestate), NULL);
							//ReadProcessMemory(processHandle, (LPVOID)(pointsAddress), &savestate2, sizeof(savestate2), NULL);
							break;
						case '2':
							//WriteProcessMemory(processHandle, (LPVOID)(addrrealpos-0x2c), &savestate, sizeof(savestate), 0);
							//WriteProcessMemory(processHandle, (LPVOID)(pointsAddress), &savestate2, sizeof(savestate2), 0);
							break;
						case '3':
							/*ReadProcessMemory(processHandle, (LPVOID)(addrrealpos-0x2c), &savestate, sizeof(savestate), NULL);
							for(int k=0; k<sizeof(savestate); k+=4){
								float j=(*(float*)&savestate[k]);
								//std::cout<<std::abs(j)<<"\n";
								if((abs(j)<0.708&&abs(j)>0.707)||(abs(j)>12.97&&abs(j)<12.98)){
								std::cout<<k<<"\t"<<round((*(float*)&savestate[k])*100000)/100000<<"\t"<<((k%16!=12)?"\t":"\n");
								}
							}*/
							//return 0;
							break;
						case '4':
							std::cout<<std::hex;
							cerr(addrPos);
							break;
//						case '5':
//							std::cin>>tempi;
//							writeMem(addrPowerup, &tempi);
//							writeMem(addrPowerup+4, &tempi);
//							break;
						default:
							break;
					}
				}
			}
		}
	}
}
