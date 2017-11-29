#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <dirent.h>

#include <opencv2/opencv.hpp>

#include <lua.hpp>

using namespace cv;

using namespace std;

#define lua_set_i(i, v) lua_pushinteger(L, v); lua_settable(L, i < 0 ? i - 2: i)
#define lua_set_t(i, v) lua_pushvalue(L, v < 0 ? v - 1 : v); lua_settable(L, i < 0 ? i - 2 : i); lua_remove(L, v)
#define lua_set_f(i, v) lua_pushcfunction(L, v); lua_settable(L, i < 0 ? i - 2 : i)
#define lua_set_s(i, v, l) lua_pushlstring(L, (const char *) v, l); lua_settable(L, i < 0 ? i - 2 : i)
#define lua_set_n(i) lua_pushnil(L); lua_settable(L, i < 0 ? i - 2 : i)

#define lua_setfield_i(i, k, v) lua_pushstring(L, #k); lua_set_i(i, v)
#define lua_setfield_t(i, k, v) lua_pushstring(L, #k); lua_set_t(i, v)
#define lua_setfield_f(i, k, v) lua_pushstring(L, #k); lua_set_f(i, v)
#define lua_setfield_s(i, k, v, l) lua_pushstring(L, #k); lua_set_s(i, v, l)
#define lua_setfield_n(i, k) lua_pushstring(L, #k); lua_set_n(i);

#define lua_setindex_i(i, k, v) lua_pushinteger(L, k); lua_set_i(i, v)
#define lua_setindex_t(i, k, v) lua_pushinteger(L, k); lua_set_t(i, v)
#define lua_setindex_f(i, k, v) lua_pushinteger(L, k); lua_set_f(i, v)
#define lua_setindex_s(i, k, v, l) lua_pushinteger(L, k); lua_set_s(i, v, l)
#define lua_setindex_n(i, k) lua_pushinteger(L, k); lua_set_n(i)

#define lua_get_i(i, v) lua_gettable(L, i < 0 ? i - 1 : i); if (!lua_isnumber(L, -1)) { lua_pop(L, 1); goto _invalid; } v = (typeof(v)) lua_tointeger(L, -1); lua_pop(L, 1)
#define lua_get_t(i, v) lua_gettable(L, i < 0 ? i - 1 : i); if (!lua_istable(L, -1)) { lua_pop(L, 1); goto _invalid; } v = -1
#define lua_get_s(i, v, l) lua_gettable(L, i < 0 ? i - 1 : i); if (!lua_isstring(L, -1)) { lua_pop(L, 1); goto _invalid; } v = (typeof(v)) lua_tolstring(L, -1, l); lua_pop(L, 1)
#define lua_get_f(i, v) lua_gettable(L, i < 0 ? i - 1 : i); if (!lua_iscfunction(L, -1)) { lua_pop(L, 1); goto _invalid; } v = lua_tocfunction(L, -1); lua_pop(L, 1)

#define lua_getfield_i(i, k, v) lua_pushstring(L, #k); lua_get_i(i, v)
#define lua_getfield_t(i, k, v) lua_pushstring(L, #k); lua_get_t(i, v)
#define lua_getfield_s(i, k, v, l) lua_pushstring(L, #k); lua_get_s(i, v, l)
#define lua_getfield_f(i, k, v) lua_pushstring(L, #k); lua_get_f(i, v)

#define lua_getindex_i(i, k, v) lua_pushinteger(L, k); lua_get_i(i, v)
#define lua_getindex_t(i, k, v) lua_pushinteger(L, k); lua_get_t(i, v)
#define lua_getindex_s(i, k, v, l) lua_pushinteger(L, k); lua_get_s(i, v, l)
#define lua_getindex_f(i, k, v) lua_pushinteger(L, k); lua_get_f(i, v)

#define lua_check(i, t, o) luaL_argcheck(L, !lua_get##t(L, i, o), i, string("expected ").append(#t).c_str())

static void lua_setrect(lua_State *L, int index, Rect& r) {
	lua_setfield_i(index, x, r.x);
	lua_setfield_i(index, y, r.y);
	lua_setfield_i(index, width, r.width);
	lua_setfield_i(index, height, r.height);
	lua_setfield_i(index, size, r.width * r.height);
}

static int lua_getrect(lua_State *L, int index, Rect& r) {
	if (!lua_istable(L, index)) goto _invalid;

	int x, y, width, height;
	lua_getfield_i(index, x, x);
	lua_getfield_i(index, y, y);
	lua_getfield_i(index, width, width);
	lua_getfield_i(index, height, height);

	r = Rect(x, y, width, height);

	return 0;

	_invalid:

	return -1;
}

static int rect_intersect(lua_State *L) {
	Rect a, b;

	lua_check(1, rect, a);
	lua_check(2, rect, b);

	a &= b;

	lua_setrect(L, 1, a);

	return 0;
}

static int rect_concat(lua_State *L) {
	Rect a, b;

	lua_check(1, rect, a);
	lua_check(2, rect, b);

	a |= b;

	lua_setrect(L, 1, a);

	return 0;
}

static int rect_intersects(lua_State *L) {
	Rect a, b;

	lua_check(1, rect, a);
	lua_check(2, rect, b);

	lua_pushboolean(L, (a & b).width);

	return 1;
}

static int rect_contains(lua_State *L) {
	Rect a, b;

	lua_check(1, rect, a);
	lua_check(2, rect, b);

	lua_pushboolean(L, a.x <= b.x &&  b.x < a.x + a.width && a.y <= b.y && b.y < a.y + a.height);

	return 1;
}

static void rect_new(Rect& r, lua_State *L) {
	lua_newtable(L);

	lua_setrect(L, -1, r);

	lua_setfield_f(-1, intersect, rect_intersect);
	lua_setfield_f(-1, concat, rect_concat);
	lua_setfield_f(-1, intersects, rect_intersects);
	lua_setfield_f(-1, contains, rect_contains);
}

static int image_index(lua_State *L) {
	if (!lua_isnumber(L, 2)) goto _invalid;

	size_t i;
	i = (size_t) lua_tointeger(L, 2);
	i--;

	size_t len;
	unsigned char *buf;
	lua_getfield_s(1, buffer, buf, &len);
	if (i >= len) goto _invalid;

	lua_pushinteger(L, buf[i]);

	return 1;

	_invalid:

	lua_pushnil(L);

	return 1;
}

static void lua_setimage(lua_State *L, int index, Mat& img) {
	lua_setfield_i(index, width, img.size().width);
	lua_setfield_i(index, height, img.size().height);
	lua_setfield_i(index, channels, img.channels());
	lua_setfield_i(index, size, img.size().width * img.size().height * img.channels());

	lua_setfield_s(index, buffer, (const char *) img.data, img.total() * img.channels());
}

static int lua_getimage(lua_State *L, int index, Mat& img) {
	if (!lua_istable(L, index)) goto _invalid;

	int width, height, channels;
	lua_getfield_i(index, width, width);
	lua_getfield_i(index, height, height);
	lua_getfield_i(index, channels, channels);

	unsigned char *buf;
	lua_getfield_s(index, buffer, buf, NULL);

	img = Mat(height, width, CV_8UC(channels), buf);

	return 0;

	_invalid:

	return -1;
}

static int image_grayscale(lua_State *L) {
	Mat img;
	lua_check(1, image, img);

	if (img.channels() > 1) {
		cvtColor(img, img, COLOR_BGR2GRAY);

		lua_setimage(L, 1, img);
	}

	return 0;
}

static int image_gaussian_blur(lua_State *L) {
	Mat img;
	lua_check(1, image, img);

	int ksize = luaL_checkint(L, 2);
	double sigma = luaL_checknumber(L, 3);

	GaussianBlur(img, img, Size(ksize, ksize), sigma, sigma);

	lua_setimage(L, 1, img);

	return 0;
}

static int image_adaptive_threshold(lua_State *L) {
	Mat img;
	lua_check(1, image, img);

	int bsize = luaL_checkint(L, 2);
	double C = luaL_checknumber(L, 3);

	adaptiveThreshold(img, img, 255, ADAPTIVE_THRESH_GAUSSIAN_C, CV_THRESH_BINARY_INV, bsize, C);

	lua_setimage(L, 1, img);

	return 0;
}

#define rect_size(r) ((r).width * (r).height)

static bool contourcmp(vector<Point> x, vector<Point> y) {
	Rect a = boundingRect(x), b = boundingRect(y);

	if (a.x == b.x) {
		return rect_size(a) > rect_size(b);
	} else {
		return a.x < b.x;
	}
}

static int image_extract_regions(lua_State *L) {
	Mat img;
	lua_check(1, image, img);

	vector<vector<Point> > contours;
	findContours(img.clone(), contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);
	sort(contours.begin(), contours.end(), contourcmp);

	lua_newtable(L);
	int i = 1;
	for (vector<vector<Point> >::iterator it = contours.begin(); it != contours.end(); it++) {	
		Rect r = boundingRect(*it);
		rect_new(r, L);
		lua_setindex_t(-2, i++, -1);
	}

	return 1;
}

static int image_resize(lua_State *L) {
	Mat img;
	lua_check(1, image, img);

	int width = luaL_checkint(L, 2);
	int height = luaL_checkint(L, 3);

	Mat _img;
	resize(img, _img, Size(width, height));

	lua_setimage(L, 1, _img);

	return 0;
}

static int image_draw_rectangle(lua_State *L) {
	Mat img;
	lua_check(1, image, img);

	if (img.channels() == 1) cvtColor(img, img, COLOR_GRAY2BGR);

	Rect rec;
	lua_check(2, rect, rec);

	int r = luaL_checkint(L, 3);
	int g = luaL_checkint(L, 4);
	int b = luaL_checkint(L, 5);

	rectangle(img, rec, Scalar(b, g, r));

	lua_setimage(L, 1, img);

	return 0;
}

static int image_show_window(lua_State *L) {
	Mat img;
	lua_check(1, image, img);

	const char *name = luaL_checkstring(L, 2);

	int flags = CV_WINDOW_NORMAL | CV_WINDOW_KEEPRATIO | CV_GUI_EXPANDED;
	if (lua_gettop(L) >= 3) {
		flags = luaL_checkint(L, 3);
	}

	namedWindow(name, flags);
	imshow(name, img);

	lua_setfield_s(1, window, name, strlen(name));

	return 0;
}

static int image_hide_window(lua_State *L) {
	Mat img;
	lua_check(1, image, img);

	const char *name;
	lua_getfield_s(1, window, name, NULL);

	destroyWindow(name);

	lua_setfield_n(1, "window");

	_invalid:

	return 0;
}

static int image_user_input(lua_State *L) {
	Mat img;
	lua_check(1, image, img);

	const char *name;
	lua_getfield_s(1, window, name, NULL);

	int key;
	key = waitKey(0);

	lua_pushinteger(L, key);

	return 1;

	_invalid:

	lua_pushnil(L);

	return 1;
}

static void image_new(Mat& img, lua_State *);

static int image_clone(lua_State *L) {
	Mat img;
	lua_check(1, image, img);

	Mat _img = img.clone();
	image_new(_img, L);

	return 1;
}

static int image_crop(lua_State *L) {
	Mat img;
	lua_check(1, image, img);

	Rect r;
	lua_check(2, rect, r);

	Mat _img;
	img(r).copyTo(_img);

	image_new(_img, L);

	return 1;
}

static void image_new(Mat& img, lua_State *L) {
	lua_newtable(L);

	lua_setimage(L, -1, img);

	lua_setfield_f(-1, grayscale, image_grayscale);
	lua_setfield_f(-1, blur, image_gaussian_blur);
	lua_setfield_f(-1, threshold, image_adaptive_threshold);
	lua_setfield_f(-1, regions, image_extract_regions);
	lua_setfield_f(-1, resize, image_resize);
	lua_setfield_f(-1, show, image_show_window);
	lua_setfield_f(-1, hide, image_hide_window);
	lua_setfield_f(-1, input, image_user_input);
	lua_setfield_f(-1, rectangle, image_draw_rectangle);
	lua_setfield_f(-1, clone, image_clone);
	lua_setfield_f(-1, crop, image_crop);

	lua_newtable(L);
	lua_setfield_f(-1, __index, image_index);
	lua_setmetatable(L, -2);

	lua_newtable(L);
	lua_newtable(L);
	lua_pushvalue(L, -3);
	lua_setfield_t(-2, __index, -1);
	lua_setmetatable(L, -2);
	lua_setfield_t(-2, data, -1);
}

static int image_load(lua_State *L) {
	size_t len;
	unsigned char *buf = (unsigned char *) luaL_checklstring(L, 1, &len);

	Mat img = imdecode(Mat(1, len, CV_8U, buf), CV_LOAD_IMAGE_COLOR);
	if (!img.data) {
		lua_pushnil(L);
		lua_pushstring(L, "unsupported or invalid image format");

		return 2;
	} else {
		image_new(img, L);

		return 1;
	}
}

static int lua_getsamples(lua_State *L, int index, vector<Mat>& samples) {
	if (!lua_istable(L, index)) return -1;

	int n = lua_objlen(L, index);

	for (int i = 1; i <= n; i++) {
		int t;
		lua_getindex_t(index, i, t);

		int m = lua_objlen(L, t);

		Mat features = Mat(1, m, CV_32FC1);
		for (int j = 0; j < m; j++) {
			lua_getindex_i(t, j + 1, features.at<float>(j));
		}

		lua_remove(L, t);

		samples.push_back(features);
	}

	return 0;

	_invalid:

	return -1;
}

static int lua_getdatabase(lua_State *L, int index, string& database) {
	if (!lua_istable(L, index)) return -1;

	const char *name;
	lua_getfield_s(index, name, name, NULL);

	database = string(name);

	return 0;

	_invalid:

	return -1;
}

/* not sure whether it's ASCII or Qt key codes */
#define KEY_BACKSPACE 0x01000003
#define KEY_RETURN 0x01000004
#define KEY_ENTER 0x01000005

static int create_response_database(lua_State *L) {
	//string name(luaL_checkstring(L, 1));
	string name;
	lua_check(1, database, name);
	string path(luaL_checkstring(L, 2));

	if (path.at(path.length() - 1) != '/') path += '/';

	DIR *d = opendir(path.c_str());
	if (!d) {
		lua_pushnil(L);
		lua_pushfstring(L, "failed to open directory '%s'", path.c_str());

		return 2;
	}
	
	ofstream ofs((name + ".rdb").c_str());
	if (ofs.bad()) {
		lua_pushnil(L);
		lua_pushfstring(L, "failed to open database '%s.rdb'", name.c_str());

		return 2;
	}
	
	int n = 0;

	struct dirent *ent;
	while ((ent = readdir(d))) {
		Mat img = imread(path + ent->d_name, CV_LOAD_IMAGE_COLOR);
		if (!img.data) continue;

		namedWindow(path + ent->d_name, CV_WINDOW_NORMAL | CV_WINDOW_KEEPRATIO | CV_GUI_EXPANDED);
		imshow(path + ent->d_name, img);

		string response;
		int key;
		do {
			key = waitKey(0);

			if (key != KEY_ENTER && key != KEY_RETURN) {
				if (key == KEY_BACKSPACE) {
					if (response.length()) response.resize(response.length() - 1);
				} else {
					/* well, this won't work for anything but a-z0-9 (not even on numpad) - or maybe it does return ASCII? */
					response += (char) key;
				}

				if (lua_isfunction(L, 3)) {
					lua_pushvalue(L, 3);
					lua_pushstring(L, response.c_str());

					if (lua_pcall(L, 1, 0, 0)) {
						ofs.close();

						closedir(d);

						const char *err = lua_tostring(L, -1);
						lua_pop(L, 1);

						return luaL_error(L, err);
					}
				}
			}
		} while (key != KEY_ENTER && key != KEY_RETURN);

		ofs << path + ent->d_name + " " << response << "\n";

		destroyWindow(path + ent->d_name);

		n++;
	}

	ofs.close();

	closedir(d);

	lua_pushinteger(L, n);

	return 1;
}

static int create_feature_database(lua_State *L) {
	//string name(luaL_checkstring(L, 1));
	string name;
	lua_check(1, database, name);
	string path(luaL_checkstring(L, 2));

	luaL_checktype(L, 3, LUA_TFUNCTION);

	if (path.at(path.length() - 1) != '/') path += '/';

	DIR *d = opendir(path.c_str());
	if (!d) {
		lua_pushnil(L);
		lua_pushfstring(L, "failed to open directory '%s'", path.c_str());

		return 2;
	}
	
	ofstream ofs((name + ".fdb").c_str());
	if (ofs.bad()) {
		lua_pushnil(L);
		lua_pushfstring(L, "failed to open database '%s.fdb'", name.c_str());

		return 2;
	}
	
	int n = 0;

	struct dirent *ent;
	while ((ent = readdir(d))) {
		Mat img = imread(path + ent->d_name, CV_LOAD_IMAGE_COLOR);
		if (!img.data) continue;

		lua_pushvalue(L, 3);
		image_new(img, L);
		if (lua_pcall(L, 1, 1, 0)) {
			const char *err = lua_tostring(L, -1);
			lua_pop(L, 1);
			
			return luaL_error(L, err);
		}

		vector<Mat> features;
		if (lua_getsamples(L, -1, features)) {
			return luaL_error(L, "expected function to return samples");
		}

		ofs << path + ent->d_name + " ";

		for (vector<Mat>::iterator it = features.begin(); it != features.end(); it++) {
			if (it != features.begin()) ofs << ";";

			for (size_t i = 0; i < it->total(); i++) {
				if (i > 0) ofs << ",";

				char f[512];
				snprintf(f, sizeof(f), "%i", (int) it->at<float>(i));
				ofs << f;
			}
		}

		ofs << "\n";

		n++;
	}

	ofs.close();

	closedir(d);

	lua_pushinteger(L, n);

	return 1;
}

static vector<string> string_tokenize(string& s, const char *d) {
	vector<string> t;

	char *_s = strdup(s.c_str());
	char *_tok, *tok = strtok_r(_s, d, &_tok);
	while (tok) {
		t.push_back(string(tok));

		tok = strtok_r(NULL, d, &_tok);
	}
	free(_s);

	return t;
}

static int load_response_database(lua_State *L) {
	//string name(luaL_checkstring(L, 1));
	string name;
	lua_check(1, database, name);

	ifstream ifs;

	ifs.open((name + ".rdb").c_str());
	if (ifs.bad()) {
		lua_pushnil(L);
		lua_pushfstring(L, "failed to open database '%s'", name.c_str());

		return 2;
	}

	lua_newtable(L);

	string line;
	for (getline(ifs, line); !ifs.eof(); getline(ifs, line)) {
		vector<string> tok = string_tokenize(line, " ");
		lua_pushstring(L, tok[1].c_str());
		lua_setfield(L, -2, tok[0].c_str());
		//lua_setfield_s(-1, tok[0].c_str(), tok[1].c_str(), tok[1].length());
	}

	ifs.close();

	return 1;
}

static int load_database(const char *name, Mat& data, Mat& responses) {
	vector<vector<string> > d;
	string r;

	ifstream ifs;

	string line;

	ifs.open(string(name).append(".fdb").c_str());
	if (ifs.bad()) return -1;

	for (getline(ifs, line); !ifs.eof(); getline(ifs, line)) {
		if (ifs.bad()) return -1;

		string features = line.substr(line.find_first_of(' ') + 1);
		vector<string> t = string_tokenize(features, ";");
		for (vector<string>::iterator it = t.begin(); it != t.end(); it++) {
			d.push_back(string_tokenize(*it, ","));
		}
	}

	ifs.close();

	data = Mat(d.size(), d.front().size(), CV_32FC1);

	for (size_t i = 0; i < d.size(); i++) {
		for (size_t j = 0; j < d[i].size(); j++) {
			data.at<float>(i, j) = (float) atoi(d[i][j].c_str());
		}
	}

	ifs.open(string(name).append(".rdb").c_str());
	if (ifs.bad()) return -1;

	for (getline(ifs, line); !ifs.eof(); getline(ifs, line)) {
		if (ifs.bad()) return -1;

		r.append(line.substr(line.find_first_of(' ') + 1));
	}

	ifs.close();

	responses = Mat(r.length(), 1, CV_32FC1);

	for (size_t i = 0; i < r.length(); i++) {
		responses.at<float>(i) = (float) r[i];
	}

	return 0;
}

/*static int retrieve_samples(lua_State *L, vector<Mat>& samples) {
	Mat img;
	lua_check(2, image, img);

	luaL_checktype(L, 3, LUA_TTABLE);

	luaL_checktype(L, 4, LUA_TFUNCTION);

	int narg;
	const char *err;

	int n = lua_objlen(L, 3);
	for (int i = 1; i <= n; i++) {
		narg = 3;
		err = "expected table<rect>";
		int t;
		lua_getindex_t(3, i, t);

		Rect r;
		if (lua_getrect(L, t, r)) goto _invalid;

		lua_remove(L, t);

		Mat roi;
		img(r).copyTo(roi);

		lua_pushvalue(L, 4);
		image_new(roi, L);
		if (lua_pcall(L, 1, 1, 0)) {
			err = lua_tostring(L, -1);
			lua_pop(L, 1);
			
			return luaL_error(L, err);
		}

		narg = 4;
		err = "expected table<rect> function(image)";
		if (!lua_istable(L, -1)) goto _invalid;

		int m = lua_objlen(L, -1);

		Mat features = Mat(1, m, CV_32FC1);
		for (int j = 0; j < m; j++) {
			lua_getindex_i(-1, j + 1, features.at<float>(j));
		}

		lua_pop(L, 1);

		samples.push_back(features);
	}

	return 0;

	_invalid:

	return luaL_error(L, "bad argument #%i to solve (%s)", narg, err);
}*/

static int database_new(lua_State *L) {
	lua_newtable(L);

	const char *name = luaL_checkstring(L, 1);

	lua_newtable(L);
	lua_setfield_s(-1, name, name, strlen(name));
	lua_setfield_f(-1, create, create_response_database);
	lua_setfield_f(-1, load, load_response_database);
	lua_setfield_t(-2, response, -1);

	lua_newtable(L);
	lua_setfield_s(-1, name, name, strlen(name));
	lua_setfield_f(-1, create, create_feature_database);
	lua_setfield_t(-2, feature, -1);

	return 1;
}

static int lua_getknn(lua_State *L, int index, CvKNearest **knn) {
	if (!lua_istable(L, index)) return -1;

	lua_getfield(L, index, "instance");
	/* shouldn't use checkudata like that, also need to pop instance field off the stack in case of error */
	//*knn = (CvKNearest *) luaL_checkudata(L, -1, "captcha.solver.knn");
	//lua_pop(L, 1);
	if (!lua_isuserdata(L, -1)) {
		lua_pop(L, 1);

		return -1;
	}
	lua_getmetatable(L, -1);
	luaL_getmetatable(L, "captcha.solver.knn");
	if (!lua_rawequal(L, -2, -1)) {
		lua_pop(L, 3);

		return -1;
	}
	lua_pop(L, 2);

	*knn = (CvKNearest *) lua_touserdata(L, -1);
	lua_pop(L, 1);

	return 0;
}

static int knn_solve(lua_State *L) {
	CvKNearest *knn;
	lua_check(1, knn, &knn);

	vector<Mat> samples;
	//retrieve_samples(L, samples);
	lua_check(2, samples, samples);

	string s;
	for (vector<Mat>::iterator it = samples.begin(); it != samples.end(); it++) {
		s += (char) knn->find_nearest(*it, 1);
	}

	lua_pushstring(L, s.c_str());

	return 1;
}

static int knn_new(lua_State *L) {
	const char *name = luaL_checkstring(L, 2);

	Mat data, responses;
	if (load_database(name, data, responses)) {
		lua_pushnil(L);
		lua_pushfstring(L, "failed to load database '%s'", name);

		return 2;
	}

	lua_newtable(L);

	CvKNearest *knn = new(lua_newuserdata(L, sizeof(CvKNearest))) CvKNearest();
	knn->train(data, responses);

	luaL_getmetatable(L, "captcha.solver.knn");
	lua_setmetatable(L, -2);

	lua_setfield(L, -2, "instance");

	lua_setfield_f(-1, solve, knn_solve);

	return 1;
}

static int knn_free(lua_State *L) {
	CvKNearest *knn = (CvKNearest *) luaL_checkudata(L, 1, "captcha.solver.knn");
	knn->~CvKNearest();

	return 0;
}

static int solver_new(lua_State *L) {
	const char *name = (string("captcha.solver.") + luaL_checkstring(L, 1)).c_str();
	//const char *database = luaL_checkstring(L, 2);

	luaL_getmetatable(L, name);
	if (!lua_isnil(L, -1)) {
		lua_CFunction new_instance;
		lua_getfield_f(-1, new, new_instance);
		lua_pop(L, 1);

		return new_instance(L);
	}

	_invalid:

	lua_pop(L, 1);

	lua_pushnil(L);
	lua_pushfstring(L, "unsupported captcha solver '%s'", name);

	return 2;
}

static const struct luaL_reg libcaptcha[] = {
	{ "load", image_load },
	{ "solver", solver_new },
	{ "database", database_new },
	{ NULL, NULL }
};

extern "C" int luaopen_captcha(lua_State *L) {
	luaL_register(L, "captcha", libcaptcha);

	luaL_newmetatable(L, "captcha.solver.knn");
	lua_setfield_f(-1, __gc, knn_free);
	lua_setfield_f(-1, new, knn_new);
	
	lua_pop(L, 1);

	return 1;
}
