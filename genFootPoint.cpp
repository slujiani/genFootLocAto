#define _USE_MATH_DEFINES
#include<iostream>
#include<fstream>
#include<sstream>
#include<vector>
#include<cmath>
#include<random>
#include<tuple>

#define vs 0.4	// 直线行走速度，每步米/秒
#define vc 0.1	// 曲线行走速度，每步米/秒
#define lcTh 1* M_PI / 180;	// 直线路径和曲线路径转角区分阈值，单位：度
using namespace std;
struct xy
{
	double x;
	double y;
};
struct posDirect
{
	xy pos;
	xy direct;
	int tag;	// 1 直线段，-1 曲线段，0 终点
};

xy vecMulC(xy a, double C)
{
	xy res = { a.x * C,a.y * C };
	return res;
}
double dotOfVec(xy a, xy b)
{
	double res = a.x * b.x + a.y * b.y;
	return res;
}
xy vecAsubB(xy a, xy b)
{
	xy res = { a.x - b.x,a.y - b.y };
	return res;
}
// str 为输入字符串，del为分隔符，返回分割之后的字符串数组
vector<double> split(string str, char del) {
	stringstream ss(str);
	string tmp;
	vector<double> res;
	while (getline(ss, tmp, del)) {
		res.push_back(stod(tmp));
	}
	return res;
}
void fileToData(vector<posDirect>& data, string filename)
{
	ifstream csvData(filename, ios::in);
	if (!csvData.is_open())
	{
		cout << "Error: opening file fail" << endl;
		exit(1);
	}
	else
	{
		string line, word;
		stringstream sin;
		posDirect newNode;
		while (getline(csvData, line))
		{
			vector<double>dataVec = split(line, ',');
			
			newNode.pos.x = dataVec[0];
			
			newNode.pos.y = dataVec[1];
			
			newNode.direct.x = dataVec[2];
			
			newNode.direct.y = dataVec[3];
			
			newNode.tag = dataVec[4];

			data.push_back(newNode);
		}
		csvData.close();
	}
}
//lcTh：直线路径和曲线路径转角区分阈值，单位：度
std::tuple<double, double, double> getRoadLen(vector<posDirect>path)
{
	int pathSize = path.size();
	double L = 0;	// 路径长度
	double Ll = 0;	// 直线长度
	double Lc = 0;	// 曲线长度
	for (int i = 0; i < pathSize-1; i++)
	{
		xy diff = vecAsubB(path[i + 1].pos, path[i].pos);
		double D = sqrt(dotOfVec(diff, diff));
		L += D;
		if (path[i].tag == -1)
		{
			Lc += D;
		}
		else
		{
			Ll += D;
		}
	}
	return std::make_tuple(L, Ll, Lc);
}
//搜索路径中离当前点最近的点，将当前点归到那一组去
int searchNearestPos(int low, int high, xy cur, vector<posDirect>path)
{
	double minp = DBL_MAX;
	int ret = -1;
	for (int i = low; i < high; i++)
	{
		xy diff = vecAsubB(path[i].pos, cur);
		double D = dotOfVec(diff, diff);
		if (D <= minp)
		{
			minp = D;
			ret = i;
		}
	}
	return ret;
}
std::tuple<posDirect, int> getOneFoot(vector<posDirect>path, int ind, posDirect cur, double v, int state_change)
{
	int pathSize = path.size();
	int lowInd = min(ind + 1, pathSize - 1);
	int i, e=pathSize-1;
	posDirect ret;
	for (i = lowInd; i < pathSize; i++)
	{
		xy diff = vecAsubB(path[i].pos, cur.pos);
		double D = dotOfVec(diff, diff);
		if (D >= pow(v, 2))
		{
			ret = path[i];
			e = i;
			break;
		}
	}
	//判断在这个过程中是否发生状态变化，如果变化，state_change== 1
	for (i = ind; i < e; i++)
	{
		if (path[i].tag != path[i + 1].tag)
		{
			state_change = 1;
			break;
		}
	}
	//如果落后足达到终点，则下一步规划还是终点位置
	if (i == pathSize-1)
	{
		ret = path[i];
	}
	return make_tuple(ret, state_change);
}
//path是落后足的路径，返回落后足及前足的点位,state_change返回值得与之前的或一下
std::tuple<posDirect, posDirect, int> getPosFit(vector<posDirect>path_behind, vector<posDirect>path_forward, posDirect cur_b, posDirect cur_f, int ind_b, int ind_f, double v_b, double v_f)
{
	posDirect pD_b, pD_f;
	//落后足从当前位置开始向前搜索，直到运动长度达到指定步长
	int pathSize = path_behind.size();
	//int lowInd = min(ind_b + 1, pathSize - 1);
	//int i, e;
	int state_change = 0;
	std::tie(pD_b, state_change) = getOneFoot(path_behind, ind_b, cur_b, v_b, state_change);
	std::tie(pD_f, state_change) = getOneFoot(path_forward, ind_f, cur_f, v_f, state_change);
	return make_tuple(pD_b, pD_f, state_change);
}
//vk=LR/LL LR\LL左右足完整轨迹中曲线段的长度
std::tuple<int, posDirect, int, posDirect, int, int> getNextLocat(vector<posDirect>left, vector<posDirect>right, posDirect Lcur, posDirect Rcur, double vk, int pll, int SC)
{
	int FR = 0;	//先走哪一足，1 左足，-1 右足，0 不动
	posDirect nextLeftF = Lcur, nextRightF = Rcur;
	double lt = 0;	//左足运动时间
	double rt = 0;
	int stateChange=0;	// state_change==1 在机器人按规划路径行走会发生状态变化，state_change==0，无状态
	int vw = 1;	//右足根据vk该比例调节步长（速度）
	//找到左足当前位置离规划的位置序列中哪个位置最近
	int pathSize = left.size();

	int lind = searchNearestPos(0, pathSize, Lcur.pos, left);
	//在左足最近位置附近，在右足序列中找到与右足位置最近的点
	int lowInd = max((int)(lind - 0.1 * pathSize), 0);
	int highInd = min((int)(lind + 0.1 * pathSize), pathSize);
	int rind = searchNearestPos(lowInd, highInd, Rcur.pos, right);

	double lv, rv;//左右脚速度
	int i;
	int e=-1;
	//两足并列
	if (abs(lind - rind) <= 3 || pll == 1)
	{
		if (lind == pathSize - 1)//已到达终点，结束
		{
			FR = 0;
			lt = 0;
			return std::make_tuple(FR, nextLeftF, lt, nextRightF, rt, stateChange);
		}
		//根据规划中下一点的状态，直线还是曲线，决定双足速度
		if (left[min(lind + 1, pathSize - 1)].tag == -1 || right[min(rind + 1, pathSize - 1)].tag == -1)
		{
			// 曲线运动
			lv = 0.5 * vc;// 左足权限运动，双足并列后，左足先动，运动半步
			rv = vk * vc;// 随后右足运动，右足速度（步长）根据曲线段长度的比例调整
		}
		else
		{
			// 直线运动
			lv = 0.5 * vs; // 左足半步
			rv = vw * vs; // 右足正常
		}
		//左足从当前位置开始，向前搜索，直到运动长度达到指定步长
		lowInd = min(lind + 1, pathSize - 1);

		for (i = lowInd; i < pathSize; i++)
		{
			xy diff = vecAsubB(left[i].pos, Lcur.pos);
			double D = dotOfVec(diff, diff);
			if (D >= lv * lv)
			{
				nextLeftF = left[i];
				lt = 0; //先运动，为0
				FR = 1;
				e = i;
				break;
			}
		}
		//判断在这个过程中是否发生状态变化，如果变化，state_change== 1

		for (i = lind; i < e; i++)
		{
			if (left[i].tag != left[i + 1].tag)
			{
				stateChange = 1;
				break;
			}
		}
		//如果左足达到终点，则下一步规划还是终点位置
		if (i == pathSize - 1)
		{
			nextLeftF = left[i];
			lt = 0;
			FR = 1;
		}
		//右足从当前位置开始向前搜索，直到运动长度达到指定步长
		lowInd = min(rind + 1, pathSize - 1);
		for (i = lowInd; i < pathSize; i++)
		{
			xy diff = vecAsubB(right[i].pos, Rcur.pos);
			double D = dotOfVec(diff, diff);
			if (D >= rv * rv)
			{
				nextRightF = right[i];
				rt = 1; //后运动，为1
				FR = 1;
				e = i;
				break;
			}
		}
		if (e == -1)
		{	//剩余长度比一步小，直接迈到终点
			e = pathSize - 1;
		}
		//判断在这个过程中是否发生状态变化，如果变化，state_change == 1
		for (i = rind; i < e; i++)
		{
			if (right[i].tag != right[i + 1].tag)
			{
				stateChange = 1;
				break;
			}
		}
		//如果右足达到终点，则下一步规划还是终点位置
		if (i == pathSize - 1)
		{
			nextRightF = right[pathSize - 1];
			rt = 1;
			FR = 1;
		}
		return std::make_tuple(FR, nextLeftF, lt, nextRightF, rt, stateChange);
	}
	//一足超前，一足落后，先运动落后足
	if (lind > rind)//左足超前,右足落后
	{
		rt = 0;
		FR = -1;
		//如果在上次运动中发生的状态变化，则强制落后足一步并列，另一足不动
		if (SC == 1 || left[lind].tag != left[min(lind + 1, pathSize - 1)].tag)
		{
			nextRightF = right[lind];
			lt = 1;//左足不动
			return std::make_tuple(FR, nextLeftF, lt, nextRightF, rt, stateChange);
		}
		//判断右足系一步的状态是直线还是曲线，决定左右足步长
		if (right[rind + 1].tag == -1)
		{
			lv = vc;
			rv = vk * vc;
		}
		else
		{
			lv = vs;
			rv = vw * vs;
		}
		std::tie(nextRightF, nextLeftF, stateChange) = getPosFit(right, left, Rcur, Lcur, rind, lind, rv, lv);
		//判断在这个过程中是否发生状态变化，如果变化，state_change == 1

	}
	else
	{
		lt = 0;
		FR = 1;
		//如果在上次运动中发生的状态变化，则强制落后足一步并列，另一足不动
		if (SC == 1 || right[lind].tag != right[min(lind + 1, pathSize - 1)].tag)
		{
			nextLeftF = left[rind];
			rt = 1;//左足不动
			return std::make_tuple(FR, nextLeftF, lt, nextRightF, rt, stateChange);
		}
		//判断右足系一步的状态是直线还是曲线，决定左右足步长
		if (left[lind + 1].tag == -1)
		{
			lv = vc;
			rv = vk * vc;
		}
		else
		{
			lv = vs;
			rv = vw * vs;
		}
		std::tie(nextLeftF, nextRightF, stateChange) = getPosFit(left, right, Lcur, Rcur, lind, rind, lv, rv);
	}
	return std::make_tuple(FR, nextLeftF, lt, nextRightF, rt, stateChange);
}
struct xyz
{
	double x;
	double y;
	double z;
};
double dotOfxyz(xyz a, xyz b)
{
	double res = a.x * b.x + a.y * b.y + a.z * b.z;
	return res;
}
xyz xyzMulC(xyz a, double C)
{
	xyz res = { a.x * C,a.y * C,a.z * C };
	return res;
}
xyz normal_xyz(xyz a)
{
	double N = sqrt(dotOfxyz(a, a));
	xyz res = xyzMulC(a, (1 / N));
	return res;
}
//计算落足点在地图坐标系中的位置和姿态
// 输入
// cur 当前落足点的坐标 x y ;方向 x y
//z：DEM地图上对应(x,y)位置的高程
//n DEM地图上(x,y,z)点的表面法线方向矢量，行向量,也是落足点法向量
//输出：
//flag：标志，1，正常，其他，异常
//vx，vy，vz：分别是足坐标系的x，y，z轴在DEM坐标中的单位向量
//R：足坐标系，x—前进方向，z—足地面发现方向，向上，y—与x，z形成右手坐标系
std::tuple<int, xyz, xyz, xyz> get_R_Q2(posDirect cur, double z, xyz n, vector<vector<double>>& R, vector<double>& Q)
{
	int flag = 1;
	double N = 1 / (sqrt(dotOfxyz(n, n)));
	n = xyzMulC(n, N);

	//计算落足点法向量与DEM坐标系Z轴夹角，如果角度大于90度，返回异常，-1；
	xyz v0 = { 0,0,1 };
	xyz v1 = n;
	xyz vx, vy, vz;
	double a0 = acos(dotOfxyz(v0, v1));
	if (abs(a0) >= M_PI / 2)
	{
		flag = -1;
		return make_tuple(flag, vx, vy, vz);
	}

	//
	double v0_3 = -(1 / n.z) * (n.x * cur.direct.x + n.y * cur.direct.y);
	//足坐标系x轴在DEM坐标中的矢量v
	xyz cur_dire_xyz = { cur.direct.x,cur.direct.y,v0_3 };
	cur_dire_xyz = normal_xyz(cur_dire_xyz);
	// 足坐标系的y轴与足坐标系x轴，z轴构成右手系
	vz = n;
	vx = cur_dire_xyz;

	vy.x = vz.y * vx.z - vz.z * vx.y;
	vy.y = vz.z * vx.x - vz.x * vx.z;
	vy.z = vz.x * vx.y - vz.y * vx.x;
	vy = normal_xyz(vy);
	//落足点坐标系在DEM坐标系中的方向（姿态）矩阵R
	R = { {vx.x,vy.x,vz.x},{vx.y,vy.y,vz.y},{vx.z,vy.z,vz.z} };
	//将方向矩阵转换成四元数表示
	//q0为四元数幅度，标量
	
	Q.push_back(0.5 * sqrt(1 + vx.x + vy.y + vz.z));
	Q.push_back( 0.25 * (vy.z - vz.y) / Q[0]);
	Q.push_back( 0.25 * (vz.x - vx.z) / Q[0]);
	Q.push_back( 0.25 * (vx.y - vy.x) / Q[0]);
	return make_tuple(flag, vx, vy, vz);
}
// 生成一个符合标准正态分布的随机数
double generateNormalRandom()
{
	// 使用标准正态分布，均值为0，方差为1
	std::normal_distribution<double> distribution(0.0, 1);
	// 创建一个随机数生成器
	std::random_device randomDevice;
	std::mt19937 generator(randomDevice());
	// 生成随机数
	return distribution(generator);
}
xy addGaussinToSim(xy goalPos)
{
	xy res;
	double ran = generateNormalRandom();
	res.x = goalPos.x + 0.005 * generateNormalRandom();
	res.y = goalPos.y + 0.005 * generateNormalRandom();
	return res;
}
void dataToFile(vector<posDirect>Path, string fname)
{
	std::ofstream outFile(fname, ios::out);
	int peSize = Path.size();
	for (int i = 0; i < peSize; i++)
	{
		outFile << to_string(Path[i].pos.x) << ','
			<< to_string(Path[i].pos.y) << ','
			<< to_string(Path[i].direct.x) << ','
			<< to_string(Path[i].direct.y) << ','
			<< to_string(Path[i].tag) << endl;
	}
	outFile.close();
}
int main()
{
	vector<posDirect> leftPath, rightPath;
	string fname1 = "leftPath.csv";
	string fname2 = "rightPath.csv";
	fileToData(leftPath, fname1);
	fileToData(rightPath, fname2);
	double lenLeft, lenLline, lenLcurve;
	double lenRight, lenRline, lenRcurve;
	std::tie(lenLeft, lenLline, lenLcurve) = getRoadLen(leftPath);
	std::tie(lenRight, lenRline, lenRcurve) = getRoadLen(rightPath);
	int FR = -1;	//到达终点的标志
	int SC = 0;	// 在行走中检测的状态变化，从曲线段到直线段变化，或相反
	int last_SC = 0;
	int pll = 1;	// 在行走过程中是否出现双足并列的情况
	double vk = lenRcurve / lenLcurve;
	posDirect nextLeftF, nextRightF;
	int stateChange, lt, rt;
	posDirect leftcur = leftPath[0];
	posDirect rightcur = rightPath[0];
	vector<posDirect>recordcalL, recordcalR;
	vector<posDirect>recordrealL, recordrealR;
	//真实值和模拟值起点都一样
	recordcalL.push_back(leftcur);
	recordcalR.push_back(rightcur);
	recordrealL.push_back(leftcur);
	recordrealR.push_back(rightcur);
	int step = 0;
	while (FR != 0)
	{
		step++;
		//根据当前落足点位置和规划的全部路径和朝向得到下一步双足的落足点位置和方向
		std::tie(FR, nextLeftF, lt, nextRightF, rt, stateChange) = getNextLocat(leftPath, rightPath, leftcur, rightcur, vk, pll, SC);
		//计算得到的下一个点  发送给机器人
		recordcalL.push_back(nextLeftF);
		recordcalR.push_back(nextRightF);

		leftcur.pos = addGaussinToSim(nextLeftF.pos);
		//leftcur.pos = nextLeftF.pos;
		leftcur.direct = nextLeftF.direct;
		leftcur.tag = nextLeftF.tag;

		rightcur.pos = addGaussinToSim(nextRightF.pos);
		//rightcur.pos = nextRightF.pos;
		rightcur.direct = nextRightF.direct;
		rightcur.tag = nextRightF.tag;
		//加高斯偏置模拟的实际值
		recordrealL.push_back(leftcur);
		recordrealR.push_back(rightcur);

		SC = stateChange;
		if (last_SC == 1 && stateChange == 0)
		{
			pll = 1;
		}
		else
		{
			pll = 0;
		}
		last_SC = stateChange;
		//按照要求的格式输出，双足的落足点在DEM地图坐标系中的位置和足坐标系在DEM地图坐标系中的姿态
		int flag;
		xyz lvx, lvy, lvz,rvx,rvy,rvz;
		xyz n = { 0,0,1 };
		vector<vector<double>>lR,rR;
		vector<double>lQ,rQ;
		std::tie(flag, lvx, lvy, lvz) = get_R_Q2(nextLeftF, 0, n, lR, lQ);
		if (flag != 1)
		{
			cout << "出错啦" << endl;
		}
		std::tie(flag, rvx, rvy, rvz) = get_R_Q2(nextRightF, 0, n, rR, rQ);
		if (flag != 1)
		{
			cout << "出错啦" << endl;
		}
	}
	string fileLsim = "file_sim_left.csv";
	string fileRsim = "file_sim_right.csv";
	string fileLreal = "file_real_left.csv";
	string fileRreal = "file_real_right.csv";
	dataToFile(recordcalL, fileLsim);
	dataToFile(recordcalR, fileRsim);
	dataToFile(recordrealL, fileLreal);
	dataToFile(recordrealR, fileRreal);
	return 0;
}