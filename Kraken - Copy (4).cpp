/*

issues
- Fix BrokerHistory3 to dowload minutes in more iterations
- Fix USD balance
- app c# download daily /hourly history
- bittrex history  https://www.reddit.com/r/BitcoinMarkets/comments/6k75ue/unable_to_get_historical_bittrex_data/
https://cryptfolio.com/historical

- websockets https://github.com/dhbaird/easywsclient

*/

#define _CRT_SECURE_NO_WARNINGS
#include "stdafx.h"

//#define FROMFILE
//#define FAKELOGIN


INITIALIZE_EASYLOGGINGPP

BOOL APIENTRY DllMain(HMODULE hModule,
DWORD  ul_reason_for_call,
LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		OutputDebugStringA("DllMain, DLL_PROCESS_ATTACH\n");
		break;
	case DLL_THREAD_ATTACH:
		OutputDebugStringA("DllMain, DLL_THREAD_ATTACH\n");
		break;
	case DLL_THREAD_DETACH:
		OutputDebugStringA("DllMain, DLL_THREAD_DETACH\n");
		break;
	case DLL_PROCESS_DETACH:
		OutputDebugStringA("DllMain, DLL_PROCESS_DETACH\n");
		break;
	default:
		OutputDebugStringA("DllMain, ????\n");
		break;
	}
	return TRUE;
}


/////////////////////////////////////////////////////////////
typedef double DATE;
//#include "D:\Zorro\include\trading.h"  // enter your path to trading.h (in your Zorro folder)
#include "..\..\Zorro\include\trading.h"
//#import "C:\\Program Files\\CandleWorks\\FXOrder2Go\fxcore.dll"  // FXCM API module

#define PLUGIN_VERSION	2
#define DLLFUNC extern "C" __declspec(dllexport)
#define LOOP_MS	200	// repeat a failed command after this time (ms)
#define WAIT_MS	10000	// wait this time (ms) for success of a command
//#define CONNECTED (g_pTradeDesk && g_pTradeDesk->IsLoggedIn() != 0)

#define MAX_STRING 1024

/////////////////////////////////////////////////////////////

int(__cdecl *BrokerError)(const char *txt) = NULL;
int(__cdecl *BrokerProgress)(const int percent) = NULL;

//HTTP func pointers prototypes
int(__cdecl *http_send)(char* url, char* data, char* header) = NULL;
long(__cdecl *http_status)(int id) = NULL;
long(__cdecl *http_result)(int id, char* content, long size) = NULL;
int(__cdecl *http_free)(int id) = NULL;


//Wrapper Func pointers declarations
typedef int(__cdecl *BROKEROPEN)(LPCSTR, FARPROC, FARPROC);
typedef int(__cdecl *BROKERTIME)(DATE *);
typedef int(__cdecl *BROKERLOGIN)(LPCSTR, LPCSTR, LPCSTR, LPCSTR);
typedef int(__cdecl *BROKERHISTORY2)(LPCSTR, DATE, DATE, int, int, T6*);
typedef int(__cdecl *BROKERASSET)(LPCSTR, double *, double *, double *, double *, double *, double *, double *, double *, double *);
typedef int(__cdecl *BROKERACCOUNT)(LPCSTR, double *, double *, double *);
typedef int(__cdecl *BROKERBUY)(LPCSTR, int, double, double *);
typedef int(__cdecl *BROKERTRADE)(int, double*, double *, double*, double*);
typedef int(__cdecl *BROKERSELL)(int, int);
//typedef int(__cdecl *BROKERSTOP)(int, double);
typedef double(__cdecl *BROKERCOMMAND)(int, DWORD);

static const char * TFARR[] = { "TIME_SERIES_INTRADAY", "TIME_SERIES_DAILY", "TIME_SERIES_WEEKLY", "TIME_SERIES_MONTLY" };

//Timezone info struct
#define pWin32Error(dwSysErr, sMsg )

typedef struct _REG_TZI_FORMAT
{
	LONG Bias;
	LONG StandardBias;
	LONG DaylightBias;
	SYSTEMTIME StandardDate;
	SYSTEMTIME DaylightDate;
} REG_TZI_FORMAT;

struct SymbolCacheItem
{
	std::string json;
	COleDateTime ExpiryTime;
};

//struct SymbolBuyInfo
//{
//	double Amount;
//	double Price;
//};

#define REG_TIME_ZONES "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones\\"
#define REG_TIME_ZONES_LEN (sizeof(REG_TIME_ZONES)-1)
#define REG_TZ_KEY_MAXLEN (REG_TIME_ZONES_LEN + (sizeof(((TIME_ZONE_INFORMATION*)0)->StandardName)/2) -1)
#define ZONE "Eastern Standard Time" //https://msdn.microsoft.com/en-us/library/ms912391(v=winembedded.11).aspx



enum TIMEFRAME
{
	TIME_SERIES_INTRADAY = 0,
	TIME_SERIES_DAILY = 1,
	TIME_SERIES_WEEKLY = 2,
	TIME_SERIES_MONTLY = 3

};

enum INTERVAL
{
	MIN1 = 1,
	MIN5 = 5,
	MIN15 = 15,
	MIN30 = 30,
	MIN60 = 60,
	MIN240 = 240,
	MIN1440 = 1440,
	MIN10080 = 10080,
	MIN21600 = 21600
};


//////////////////////// DLL Globals
static int g_nLoggedIn = 0;
static HINSTANCE hinstLib = 0;
Dictionary<std::string, SymbolCacheItem> SymbolDic;
Dictionary<std::string, double> SymbolLotAmount;
Dictionary<std::string, double> SymbolLastPrice;
//Dictionary<std::string, double> SymbolBuyInfo;
Dictionary<unsigned int, double> DemoTradesID;
std::string g_PrivateKey = "";
std::string g_PublicKey = "";
std::string g_DisplayCurrency = "";
std::string g_BaseTradeCurrency = "";
std::string g_AssetPairs = "";
std::string g_MinOrderSize = "";
int g_DemoTrading = 0;
int g_IntervalSec = 0;
int g_EnableLog = 0;
double g_BaseCurrAccCurrConvRate = 1;
int g_UseCryptoCompareHistory = 0;

std::string _KrakenUrl = "https://api.kraken.com";

////////////////////////////////////////////////////////////////
HINSTANCE LoadDLL()
{
	HINSTANCE hinstLib = 0;
	//hinstLib = LoadLibrary(TEXT("D:\\Zorro\\Plugin\\IB.dll"));
	hinstLib = LoadLibrary(TEXT(".\\Plugin\\IB.dll"));
	if (hinstLib != NULL)
		return hinstLib;
	else
		return 0;
}

//DATE format (OLE date/time) is a double float value, counting days since midnight 30 December 1899, while hours, minutes, and seconds are represented as fractional days. 
DATE convertTime(__time32_t t32)
{
	return (double)t32 / (24.*60.*60.) + 25569.; // 25569. = DATE(1.1.1970 00:00)
}

// number of seconds since January 1st 1970 midnight: 
__time32_t convertTime(DATE date)
{
	return (__time32_t)((date - 25569.)*24.*60.*60.);
}


//https://stackoverflow.com/questions/24752141/hmac-sha512-bug-in-my-code
//https://www.freeformatter.com/hmac-generator.html#ad-output
//http://www.rohitab.com/discuss/topic/39777-hmac-md5sha1/
//http://www.drdobbs.com/security/the-hmac-algorithm/184410908


//http://stackoverflow.com/questions/466071/how-do-i-get-a-specific-time-zone-information-struct-in-win32
int GetTimeZoneInformationByName(TIME_ZONE_INFORMATION *ptzi, const char StandardName[])
{
	int rc;
	HKEY hkey_tz;
	DWORD dw;
	REG_TZI_FORMAT regtzi;
	char tzsubkey[REG_TZ_KEY_MAXLEN + 1] = REG_TIME_ZONES;

	strncpy(tzsubkey + REG_TIME_ZONES_LEN, StandardName, sizeof(tzsubkey) - REG_TIME_ZONES_LEN);
	if (tzsubkey[sizeof(tzsubkey) - 1] != 0) {
		fprintf(stderr, "timezone name too long\n");
		return -1;
	}

	if (ERROR_SUCCESS != (dw = RegOpenKeyA(HKEY_LOCAL_MACHINE, tzsubkey, &hkey_tz))) {
		fprintf(stderr, "failed to open: HKEY_LOCAL_MACHINE\\%s\n", tzsubkey);
		pWin32Error(dw, "RegOpenKey() failed");
		return -1;
	}

	rc = 0;
#define X(param, type, var) \
        do if ((dw = sizeof(var)), (ERROR_SUCCESS != (dw = RegGetValueW(hkey_tz, NULL, param, type, NULL, &var, &dw)))) { \
            fprintf(stderr, "failed to read: HKEY_LOCAL_MACHINE\\%s\\%S\n", tzsubkey, param); \
            pWin32Error(dw, "RegGetValue() failed"); \
            rc = -1; \
            goto ennd; \
						        } while(0)
	X(L"TZI", RRF_RT_REG_BINARY, regtzi);
	X(L"Std", RRF_RT_REG_SZ, ptzi->StandardName);
	X(L"Dlt", RRF_RT_REG_SZ, ptzi->DaylightName);
#undef X
	ptzi->Bias = regtzi.Bias;
	ptzi->DaylightBias = regtzi.DaylightBias;
	ptzi->DaylightDate = regtzi.DaylightDate;
	ptzi->StandardBias = regtzi.StandardBias;
	ptzi->StandardDate = regtzi.StandardDate;
ennd:
	RegCloseKey(hkey_tz);
	return rc;
}


void Log(std::string message, int Level, bool LogZorro)
{

	if (g_EnableLog == 1)
	{
		if (Level == 0)
			LOG(INFO) << message.c_str();
		else if (Level == 1)
			LOG(ERROR) << message.c_str();
	}

	if (LogZorro)
	{
		BrokerError(message.c_str());
	}

}


std::string ftoa(double val)
{
	char cVal[30];
	sprintf(cVal, "%0.10f", val);
	return std::string(cVal);
}

std::string itoa(int val)
{
	char cVal[30];
	sprintf(cVal, "%d", val);
	return std::string(cVal);
}

std::string uitoa(unsigned int val)
{
	char cVal[30];
	sprintf(cVal, "%u", val);
	return std::string(cVal);
}

bool isNumber(std::string line)
{
	char* p;
	strtol(line.c_str(), &p, 10);
	return *p == 0;
}

std::string GetPublicApiKey()
{
	return g_PublicKey;
}

std::string GetNonce()
{
	SYSTEMTIME st;
	GetSystemTime(&st);
	//printf("\n In C++ : %04d:%02d:%02d:%02d:%02d:%02d:%03d\n", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	st.wYear = st.wYear + 1600;

	//TODO remove
	//st.wHour = st.wHour + 10;

	FILETIME fileTime;
	SystemTimeToFileTime(&st, &fileTime);

	long long ticks = (((ULONGLONG)fileTime.dwHighDateTime) << 32) + fileTime.dwLowDateTime;

	std::string strNonce = "000000000000000000";
	sprintf(&strNonce[0], "%I64u" /*"%llu"*/, ticks);

	return strNonce;

}

std::string GetAPISignature(std::string ApiPath, std::string PostData, std::string nonce)
{
	std::string ret = "";
	std::string PublicKey = GetPublicApiKey();
	std::string PrivateKey = g_PrivateKey;
	std::string param = nonce + PostData;

	unsigned char digest[SHA256::DIGEST_SIZE];
	memset(digest, 0, SHA256::DIGEST_SIZE);
	SHA256 ctx = SHA256();
	ctx.init();
	ctx.update((unsigned char*)param.c_str(), param.length());
	ctx.final(digest);


	//TODO do a byte array di ApiPath + sha256(param)
	const int len = ApiPath.length() + SHA256::DIGEST_SIZE;
	unsigned char * path_param;
	path_param = new unsigned char[len];
	memset(path_param, 0, len);
	memcpy(path_param, ApiPath.c_str(), ApiPath.length());
	memcpy(path_param + ApiPath.length(), digest, SHA256::DIGEST_SIZE);



	unsigned char hmac_512[SHA512::DIGEST_SIZE];
	memset(hmac_512, 0, SHA512::DIGEST_SIZE);
	std::string encp = base64_decode(PrivateKey);
	HMAC512(encp, path_param, len, hmac_512);

	ret = base64_encode(hmac_512, SHA512::DIGEST_SIZE);


	delete[] path_param;

	return ret;
}

std::string GetHeader(std::string ApiPath, std::string PostData, std::string nonce)
{
	std::string sign = GetAPISignature(ApiPath, PostData, nonce);
	std::ostringstream ss;
	ss << "{Content-Type: application/x-www-form-urlencoded\n";
	ss << "API-Key: " << GetPublicApiKey() << "\n";
	ss << "API-Sign: " << sign.c_str() << "\n}";
	//ss << "Host: api.kraken.com" << "\r\n}";

	return ss.str();

}

std::string GetAsset(char * Asset)
{
	//Remove backslash for currencies
	std::string sAsset = std::string(Asset);
	sAsset.erase(std::remove(sAsset.begin(), sAsset.end(), '/'), sAsset.end());
	return sAsset;
}

unsigned int GetTradeID()
{
	static unsigned int last;
	time_t ltime;
	time(&ltime);
	if (ltime == last)
		ltime++;
	else if (ltime < last)
	{
		ltime = last;
		ltime++;
	}
	last = ltime;
	return ltime;

}

std::string GetTradeIDstring()
{
	unsigned int tid = GetTradeID();
	char temp[20];
	_itoa(tid, temp, 10);
	return std::string(temp);
}



void ClearCache()
{
	SymbolDic.Clear();
}

int AddToCache(std::string url, std::string json)
{
	if (g_IntervalSec <= 0)
		return 0;

	try
	{
		SymbolCacheItem *jsonCached = SymbolDic.TryGetValue(url);
		if (jsonCached)
			SymbolDic.Remove(url);

		SymbolCacheItem newitem;
		newitem.json = json;
		COleDateTime currTime = COleDateTime::GetCurrentTime();
		COleDateTimeSpan ts(0,0,0,g_IntervalSec);
		COleDateTime exptime = currTime + ts;
		CString sStart = currTime.Format(_T("%A, %B %d, %Y %H:%M:%S"));
		CString sEnd = exptime.Format(_T("%A, %B %d, %Y %H:%M:%S"));
		newitem.ExpiryTime = exptime;
		SymbolDic.Add(url, newitem);
		return 1;
	}
	catch (...)
	{
		Log("AddToCache Exception", 1,false);
		return 0;
	}
}

int GetFromCache(std::string url, std::string &json)
{
	try
	{
		SymbolCacheItem *jsonCached = SymbolDic.TryGetValue(url);
		if (jsonCached)
		{
			COleDateTime currTime = COleDateTime::GetCurrentTime();
			if (!jsonCached->ExpiryTime.GetStatus() == COleDateTime::valid)
				return 0;

			if (currTime > jsonCached->ExpiryTime)
				return 0;
			json = jsonCached->json;
			return 1;
		}
		else
			return 0;
	}
	catch (...)
	{
		Log("GetFromCache Exception", 1,false);
		return 0;
	}
}

void RemoveFromCache(std::string url)
{
	try
	{
		SymbolCacheItem *jsonCached = SymbolDic.TryGetValue(url);
		if (jsonCached)
			SymbolDic.Remove(url);
	}
	catch (...)
	{
		Log("RemoveFromCache Exception", 1, false);

	}
}

int CallAPI(std::string ApiPath, std::string& json, std::string symbol, std::string data, bool UseCache, bool UseKrakenRoot=true)
{

	int id = 0;
	int length = 0;
	int counter = 0;

	
	std::string URL = "";
	if (UseKrakenRoot)
		URL = _KrakenUrl + ApiPath;
	else
		URL = ApiPath;
	std::string CacheKey =URL + "?" + data;
	std::string header="";
	if (!data.empty() || data!="")
	{
		std::string nonce = GetNonce();
		data = "nonce=" + nonce + "&" + data;
		header = GetHeader(ApiPath, data,nonce);
		Log(header, 0, false);
	}
	


#ifdef FROMFILE
	//std::ifstream t("..\..\Zorro\json.txt");
	std::ifstream t("json.txt");
	std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
	json.assign(str);
	length = json.length();
#else

	while (length <= 3)
	{
		if (UseCache)
		{
			if (GetFromCache(CacheKey, json))
				return 1;
		}

		if (data == "")
			id = http_send((char*)URL.c_str(), NULL, NULL);
		else
			id = http_send((char*)URL.c_str(), (char*)data.c_str(), (char*)header.c_str());

		if (!id)
			return 0;
		while (!http_status(id))
		{
			if (!SleepEx(100, FALSE))
				return 0; // wait for the server to reply
		}

		length = http_status(id);
		if (length <= 3)
			http_free(id);

		if (counter > 3)
		{
			Log("Maximum Retry calling http_send reached.", 1,true);
			break;
		}
		counter++;
	}

#endif


	if (length > 3)
	{ //transfer successful?

#ifndef FROMFILE
		string content = (string)malloc(length+1);
		ZeroMemory(content, length );
		http_result(id, content, length ); // store price data in large string
		json.assign(content);
		if (UseCache && json.length()>50)
			AddToCache(CacheKey, content);
		free(content);
#ifdef _DEBUG
		Log(URL, 0, false);
		//Log((char *)json.c_str(),0);
#endif
		http_free(id);
#endif
	}
	else
	{
		BrokerError("Error calling web service");
#ifndef FROMFILE
		http_free(id); //always clean up the id!
#endif
		return 0;
	}

	return 1;
}

int Parse(std::string ApiPath, picojson::value::object& objJson, std::string& json, std::string symbol, std::string data)
{
	try
	{
		picojson::value jsonParsed;
		if (ApiPath.find("/AssetPairs") == std::string::npos && ApiPath.find("/OHLC") == std::string::npos && ApiPath.find("/histo") == std::string::npos)
			Log(json, 0, false);

		std::string err = picojson::parse(jsonParsed, json);
		if (!err.empty())
		{
			Log(err, 1,true);
			return 0;
		}


		// check if the type of the value is "object"
		if (!jsonParsed.is<picojson::object>())
		{
			Log(ApiPath + " - JSON is not an object", 1,true);
			return 0;
		}

		objJson = jsonParsed.get<picojson::object>();
		if (objJson.begin()->first == "error")
		{

			const picojson::value::array& objErrArr = objJson.begin()->second.get<picojson::array>();
			for (picojson::value::array::const_iterator iC = objErrArr.begin(); iC != objErrArr.end(); iC++)
			{
				if (iC == objErrArr.end())
					continue;

				Log(iC->to_str(), 1,true);
				Log(ApiPath, 1, false);
				Log(data, 1, false);

			}

			int size = objErrArr.size();
			if (size > 0)
				return 0;
		}


	}
	catch (const std::exception& e)
	{
		Log(e.what(), 0,true);
		return 0;
	}

	return 1;
}

int CallAPIAndParse(std::string ApiPath, picojson::value::object& objJson, std::string symbol, std::string data, bool UseCache,bool UseKrakenRoot = true)
{

	try
	{

		std::string json;
		if (!CallAPI(ApiPath, json, symbol, data, UseCache, UseKrakenRoot))
		{
			Log(ApiPath + " - Error calling API", 1,true);
			return 0;
		}

		if (!Parse(ApiPath, objJson, json, symbol, data))
		{
			Log(ApiPath + " - Error Parsing JSon", 1,true);
			return 0;
		}

	}
	catch (const std::exception& e)
	{
		Log((char *)e.what(), 1, true);
		return 0;
	}

	return 1;
}

/*
0 when the connection to the server was lost, and a new login is required.
1 when the connection is ok, but the market is closed or trade orders are not accepted.
2 when the connection is ok and the market is open for trading at least one of the subscribed assets.
*/
DLLFUNC int BrokerTime(DATE *pTimeGMT);
int IsLoggedIn()
{

#ifdef FAKELOGIN
	return 1;
#endif

	DATE date = 0;
	return g_nLoggedIn;
}

double GetAccBaseCurrExchRate(std::string asset)
{
	Log("GetAccBaseCurrExchRate IN Symbol: " + asset, 0, false);
	double price = 0;


	int ret = 0;
	std::string ApiPath = "/0/public/Ticker?pair=" + asset;

	try
	{
		picojson::value::object objJson;
		if (!CallAPIAndParse(ApiPath, objJson, asset, "", false))
		{
			Log("Error calling API GetAccBaseCurrExchRate", 1, true);
			Log("GetAccBaseCurrExchRate OUT", 0, false);
			return 0;
		}


		picojson::value::object::const_iterator iA = objJson.begin(); ++iA;//iA shoud be Time Series Object
		const picojson::value::object& objPair = iA->second.get<picojson::object>();
		std::string sPair = objPair.begin()->first.c_str();

		const picojson::value::array&  askArray = objPair.begin()->second.get("a").get<picojson::array>();
		picojson::value::array::const_iterator iG = askArray.begin();
		price = atof(iG->to_str().c_str());

	}
	catch (const std::exception& e)
	{
		Log(e.what(), 1, true);
		Log("GetAccBaseCurrExchRate OUT", 0, false);
		return 0;
	}


	Log("GetAccBaseCurrExchRate OUT", 0, false);
	return price;


	//XBT/USD
	//return 3913.889;
}

//https://support.kraken.com/hc/en-us/articles/205893708-What-is-the-minimum-order-size-
double GetMinOrderSize(std::string asset)
{
	try
	{
		std::string base = "";
		int pos = asset.find("X" + g_BaseTradeCurrency);
		if (pos > 0)
		{
			base = asset.substr(1, pos - 1);
		}
		if (pos==-1)
		{ 
			pos = asset.find(g_BaseTradeCurrency);
			base = asset.substr(0, pos);
		}
			

		

		picojson::value::object objValues;
		if (g_MinOrderSize != "")
		{

			if (!Parse("", objValues, g_MinOrderSize, base, ""))
			{
				std::string err = ("Error parsing MinOrderSize json");
				Log(err, 1, true);
				throw std::invalid_argument(err.c_str());
			}

			picojson::value::object::const_iterator iJ = objValues.begin(); ++iJ;
			const picojson::value& objAssetPairVal = iJ->second.get(base);
			double ret = iJ->second.get(base).get<double>();
			return ret;



		}
		else
		{
			std::string err = ("Base currency not found: " + base);
			throw std::invalid_argument(err.c_str());
		}

	}
	catch (const std::exception& e)
	{

		Log("Error MinOrderSize missing. " + std::string(e.what()), 1, true);
		Log("BrokerAsset OUT", 0,false);
		return 0;
	}

	/*if (base == "REP")
		return 0.3;
	else if (base == "XBT")
		return 0.002;
	else if (base == "DASH")
		return 0.03;
	else if (base == "XDG")
		return 3000;
	else if (base == "EOS")
		return 2;
	else if (base == "ETH")
		return 0.02;
	else if (base == "ETC")
		return 0.3;
	else if (base == "GNO")
		return 0.03;
	else if (base == "ICN")
		return 2;
	else if (base == "LTC")
		return 0.1;
	else if (base == "MLN")
		return 0.1;
	else if (base == "XMR")
		return 0.1;
	else if (base == "XRP")
		return 30;
	else if (base == "XLM")
		return 300;
	else if (base == "ZEC")
		return 0.03;
	else
	{
		std::string err = ("Base currency not found: " + base);
		throw std::invalid_argument(err.c_str());
	}*/
}



int CanSubscribe(std::string& Asset)
{
	Log("CanSubscribe IN", 0,false);

	if (!IsLoggedIn())
		return 0;

	std::string ApiPath = "/0/public/AssetPairs";

	int pos = Asset.find(g_BaseTradeCurrency);
	if (pos < 3)
	{
		Log("Base trade currency must be the right one on the pair.", 1,true);
		Log("CanSubscribe OUT", 0,false);
		return 0;
	}


	try
	{
		picojson::value::object objJson;
		if (g_AssetPairs == "")
		{

			if (!CallAPIAndParse(ApiPath, objJson, Asset, "", true))
			{
				Log("Error calling API CanSubscribe", 1,true);
				Log("CanSubscribe OUT", 0,false);
				return 0;
			}

			picojson::value::object::const_iterator i1 = objJson.begin(); ++i1;
			g_AssetPairs = "{\"error\":[],\"result\":" + i1->second.serialize() + "}";

			
		}
		else
		{
			if (!Parse(ApiPath, objJson, g_AssetPairs, Asset, ""))
			{
				Log("Error calling API CanSubscribe", 1,true);
				Log("CanSubscribe OUT", 0,false);
				return 0;
			}
		}



		picojson::value::object::const_iterator iA = objJson.begin(); ++iA;
		std::string valret = iA->second.serialize();
		int found = valret.find("\"" + Asset + "\"");

		if (found == -1)
			return 0;
		else
			return 1;


	}
	catch (const std::exception& e)
	{
		Log(e.what(),1,true);
		Log("CanSubscribe OUT", 0,false);
		return 0;
	}


	Log("CanSubscribe OUT", 0,false);
	return 1;
}




/*
[ALPHAADVANTAGE]
AlphaAdvantageKey = "????"
AlphaAdvantageIntervalMin = "15"*/
std::string ReadZorroIniConfig(std::string key)
{
	char bufDir[300];
	GetCurrentDirectoryA(300, bufDir);
	strcat(bufDir, "\\Zorro.ini");

	char buf[300];
	int success = GetPrivateProfileStringA("KRAKEN", key.c_str(), "", buf, 300, bufDir);
	std::string ret = std::string(buf);
	if (ret == std::string(""))
	{
		BrokerError(key.c_str());
		BrokerError(" not set in Zorro.ini");
	}

	return ret;
}

////////////////////////////////////////////////////////////////

DLLFUNC int BrokerHTTP(FARPROC fp_send, FARPROC fp_status, FARPROC fp_result, FARPROC fp_free)
{
	(FARPROC&)http_send = fp_send;
	(FARPROC&)http_status = fp_status;
	(FARPROC&)http_result = fp_result;
	(FARPROC&)http_free = fp_free;
	return 1;
}

int BrokerHistory3(std::string sAsset, DATE tStart, DATE tEnd, int nTickMinutes, int nTicks, T6* ticks)
{
	Log("BrokerHistory3 IN", 0, false);
	//TODO Remove
	//nTickMinutes = 1440;


	std::string fsym = "";
	int len = 0;
	int pos = sAsset.find("X" + g_BaseTradeCurrency);
	if (pos > 0)
	{
		fsym = sAsset.substr(1, pos - 1);
	}
	if (pos == -1)
	{
		pos = sAsset.find(g_BaseTradeCurrency);
		fsym = sAsset.substr(0, pos);
	}
	len = fsym.length();
	if (fsym == "XBT")
		fsym = "BTC";
	else if (fsym == "XDG")
		fsym = "DOGE";

	std::string tsym = sAsset.substr(pos, sAsset.length()-len);
	if (tsym == "XBT")
		tsym = "BTC";
	else if (tsym == "XDG")
		tsym = "DOGE";
	
	std::string aggregate = "1";
	std::string action = "histominute";
	if (nTickMinutes == (int)MIN1 || nTickMinutes == (int)MIN15 || nTickMinutes == (int)MIN30)
	{
		action = "histominute";
		aggregate = itoa(nTickMinutes);
	}
	else if (nTickMinutes == (int)MIN60 || nTickMinutes == (int)MIN240)
	{
		action = "histohour";
		aggregate = "1";
		if (nTickMinutes == (int)MIN240)
			aggregate = "4";
	}
	else if (nTickMinutes == (int)MIN1440 || nTickMinutes != (int)MIN10080 || nTickMinutes != (int)MIN21600)
	{
		action = "histoday";
		aggregate = "1";
		if (nTickMinutes == (int)MIN10080)
			aggregate = "7";
		if (nTickMinutes == (int)MIN21600)
			aggregate = "30";
	}

	COleDateTime odtStart(tStart);
	COleDateTime odtEnd(tEnd);
	CStringA sStart = odtStart.Format(TEXT("%c"));
	CStringA sEnd = odtEnd.Format(TEXT("%c"));
	std::string toTs =itoa( convertTime(tEnd));

	std::string limit = uitoa(nTicks);
	int nTick = 0;
	T6* tick = ticks;

	try
	{
		//while (convertTime((__time32_t)(atol(toTs.c_str()))) >tStart)
		//{
			std::string ApiPath = "https://min-api.cryptocompare.com/data/" + action + "?fsym=" + fsym + "&tsym=" + tsym + "&limit=" + limit + "&aggregate=" + aggregate + "&e=CCCAGG&toTs=" + toTs;

			picojson::value::object objJson;
			if (!CallAPIAndParse(ApiPath, objJson, sAsset, "", true, false))
			{
				Log("Error calling API BrokerHistory3", 1, true);
				Log("BrokerHistory3 OUT", 0, false);
				return 0;
			}

			const picojson::value::array& objData = objJson.at("Data").get<picojson::array>();

			
			//Start from the most recent quote
			for (picojson::value::array::const_iterator iC = objData.end(); iC != objData.begin(); --iC)
			{
				if (iC == objData.end())
					continue;

				if (iC->get("close").to_str() == "0")
					continue;

				//Set time
				COleDateTime tTickTime;
				tTickTime = convertTime((__time32_t)(atol(iC->get("time").to_str().c_str())));
				toTs = iC->get("time").to_str();
				if (tTickTime.m_dt<tStart || tTickTime.m_dt>tEnd)
					continue;
				tick->time = tTickTime.m_dt;

				//Set Open
				tick->fOpen = atof(iC->get("open").to_str().c_str());
				//Set High
				tick->fHigh = atof(iC->get("high").to_str().c_str());
				//Set Low
				tick->fLow = atof(iC->get("low").to_str().c_str());
				//Set Close
				tick->fClose = atof(iC->get("close").to_str().c_str());
				//Set Volume
				tick->fVol = atof(iC->get("volumeto").to_str().c_str());

				if (!BrokerProgress(100 * nTick / nTicks))
					break;


				if (nTick == nTicks /*- 1*/)
					break;

				tick++;
				nTick++;

			}
		//}
	}
	catch (const std::exception& e)
	{
		Log(e.what(), 1, true);
		Log("BrokerHistory3 OUT", 0, false);
		return 0;
	}
	

	Log("return nTick: " + itoa(nTick), 0, false);
	Log("BrokerHistory3 OUT", 0, false);
	return nTick;
}

//DLLFUNC int BrokerHistory(char* Asset, DATE tStart, DATE tEnd, int nTickMinutes, int nTicks, TICK* ticks)
DLLFUNC int BrokerHistory2(char* Asset, DATE tStart, DATE tEnd, int nTickMinutes, int nTicks, T6* ticks)
{
	Log("BrokerHistory2 IN", 0,false);

	if (!IsLoggedIn())
		return 0;

	std::string sAsset = GetAsset(Asset);
	
	COleDateTime odtStart(tStart);
	COleDateTime odtEnd(tEnd);
	CStringA sStart = odtStart.Format(TEXT("%c"));
	CStringA sEnd = odtEnd.Format(TEXT("%c"));
	COleDateTimeSpan diff = odtEnd - odtStart;
	char sTickMinutes[8];
	_itoa(nTickMinutes, sTickMinutes,10);
	char sTicks[8];
	_itoa(nTicks, sTicks, 10);
	int TotalMinutes = diff.GetTotalMinutes();
	int TotalDays = diff.GetTotalDays();
	CStringA logstring = "Params " + CStringA(sAsset.c_str()) + " Start : " + sStart + " End : " + sEnd + " nTickMinutes : " + CStringA(sTickMinutes) + " nTicks : " + CStringA(sTicks);
	std::string logstring2((char*)(LPCTSTR)logstring.GetString());
	Log(logstring2, 0,false);

	if (nTickMinutes == 0)
		nTickMinutes = 1;
	if (nTickMinutes != (int)MIN1 && nTickMinutes != (int)MIN5 && nTickMinutes != (int)MIN15 && nTickMinutes != (int)MIN30 && nTickMinutes != (int)MIN60 &&  nTickMinutes != (int)MIN240  && nTickMinutes != (int)MIN1440  && nTickMinutes != (int)MIN10080  && nTickMinutes != (int)MIN21600)
	{
		Log("TimeFrame not supported", 1, true);
		return 0;
	}

	if (g_UseCryptoCompareHistory)
		return BrokerHistory3(sAsset, tStart, tEnd, nTickMinutes, nTicks, ticks);


	int interval = nTickMinutes;

	int tokensize = TotalMinutes / nTickMinutes;
	//if (tokensize > nTicks)
	//	tokensize = nTicks;
	char cTickMinutes[30];
	_itoa(nTickMinutes, cTickMinutes, 10);
	const int outputsize = tokensize;
	std::string ApiPath = "/0/public/OHLC?pair=" + sAsset + "&interval=" + std::string(cTickMinutes);


	int nTick = 0;
	try
	{

		picojson::value::object objJson;
		if (!CallAPIAndParse(ApiPath, objJson, sAsset, "", true))
		{
			Log("Error calling API BrokerHistory2", 1,true);
			Log("BrokerHistory2 OUT", 0,false);
			return 0;
		}

		picojson::value::object::const_iterator iA = objJson.begin(); ++iA;//iA shoud be Time Series Object
		const picojson::value::object& objTimeSeries = iA->second.get<picojson::object>();
		picojson::value::object::const_iterator iB = objTimeSeries.begin(); //++iB;//iA shoud be Time Series Object
		const picojson::value::array& objTimeSeriesArray = iB->second.get<picojson::array>();

		T6* tick = ticks;
		//Start from the most recent quote
		for (picojson::value::array::const_iterator iC = objTimeSeriesArray.end(); iC != objTimeSeriesArray.begin(); --iC)
		{
			if (iC == objTimeSeriesArray.end())
				continue;

			//array of array entries(<time>, <open>, <high>, <low>, <close>, <vwap>, <volume>, <count>)
			const picojson::value::array& ohlcArray = iC->get<picojson::array>();
			picojson::value::array::const_iterator iF = ohlcArray.begin();

			//Set time
			COleDateTime tTickTime;
			tTickTime = convertTime((__time32_t)(atol(iF->to_str().c_str())));
			if (tTickTime.m_dt<tStart || tTickTime.m_dt>tEnd)
				continue;
			tick->time = tTickTime.m_dt;
			iF++;

			//Set Open
			tick->fOpen = atof(iF->to_str().c_str());
			iF++;
			//Set High
			tick->fHigh = atof(iF->to_str().c_str());
			iF++;
			//Set Low
			tick->fLow = atof(iF->to_str().c_str());
			iF++;
			//Set Close
			tick->fClose = atof(iF->to_str().c_str());
			iF++;
			//Set Volume
			tick->fVol = atof(iF->to_str().c_str());



			if (!BrokerProgress(100 * nTick / nTicks))
				break;


			if (nTick == nTicks - 1)
				break;

			tick++;
			nTick++;

		}
	}
	catch (const std::exception& e)
	{
		Log(e.what(),1,true);
		Log("BrokerHistory2 OUT", 0,false);
		return 0;
	}

	Log("BrokerHistory2 OUT", 0,false);
	return nTick;
}

//////////////////////////////////////////////////////////////////
//http://www.alphavantage.co/documentation/    "http://www.alphavantage.co/query?function=TIME_SERIES_INTRADAY&symbol=MSFT&interval=1min&outputsize=300&apikey=2985"

//https://bitbucket.org/yarosla/nxjson
//https://github.com/zserge/jsmn

DLLFUNC int BrokerAsset(char* Asset, double* pPrice, double* pSpread, double *pVolume, double *pPip, double *pPipCost, double *pMinAmount,
	double *pMargin, double *pRollLong, double *pRollShort)
{

	Log("BrokerAsset IN Symbol: " + std::string(Asset), 0,false);

	if (!IsLoggedIn())
		return 0;

	std::string sAsset = GetAsset(Asset);
	std::string logDetails ="Asset Details: " + sAsset + " ";

	//Subscribe the asset
	if (!pPrice)
		return CanSubscribe(sAsset);
	else
		*pPrice = 0.;

	if (pVolume)
		*pVolume = 0.;

	int ret = 0;
	std::string ApiPath = "/0/public/Ticker?pair=" + sAsset;

	try
	{
		picojson::value::object objJson;
		if (!CallAPIAndParse(ApiPath, objJson, sAsset, "", false))
		{
			Log("Error calling API BrokerAsset", 1,true);
			Log("BrokerAsset OUT", 0,false);
			return 0;
		}


		picojson::value::object::const_iterator iA = objJson.begin(); ++iA;//iA shoud be Time Series Object
		const picojson::value::object& objPair = iA->second.get<picojson::object>();
		std::string sPair = objPair.begin()->first.c_str();

		const picojson::value::array&  askArray = objPair.begin()->second.get("a").get<picojson::array>();
		picojson::value::array::const_iterator iG = askArray.begin();
		if (pPrice)
		{
			*pPrice = atof(iG->to_str().c_str());
			logDetails = logDetails + "Price: " + ftoa(*pPrice) + " ";
		}
		double *SymbolLastPriceVal = SymbolLastPrice.TryGetValue(sAsset);
		if (!SymbolLastPriceVal)
			SymbolLastPrice.Add(sAsset, atof(iG->to_str().c_str()));
		else
			*SymbolLastPriceVal = atof(iG->to_str().c_str());

		const picojson::value::array&  bidArray = objPair.begin()->second.get("b").get<picojson::array>();
		picojson::value::array::const_iterator iF = bidArray.begin();
		if (pSpread)
		{
			*pSpread = *pPrice - atof(iF->to_str().c_str());
			logDetails = logDetails + "Spread: " + ftoa(*pSpread) + " ";
		}

		const picojson::value::array&  volArray = objPair.begin()->second.get("v").get<picojson::array>();
		picojson::value::array::const_iterator iH = volArray.begin(); iH++;
		double volume = atof(iH->to_str().c_str()) / 1440;//this is the last 24h volume, so as I need the minute vol, is devided by 1440
		if (pVolume)
		{
			*pVolume = volume;
			logDetails = logDetails + "Volume: " + ftoa(*pVolume) + " ";
		}


		////////////////  Parse the Assetpairs json  ////////////////////////
		picojson::value::object objAssetpairs;

		if (!Parse("/AssetPairs", objAssetpairs, g_AssetPairs, sAsset, ""))
		{
			Log("Error calling API BrokerAsset", 1,true);
			Log("BrokerAsset OUT", 0,false);
			return 0;
		}


		picojson::value::object::const_iterator iJ = objAssetpairs.begin(); ++iJ;
		const picojson::value& objAssetPairVal = iJ->second.get(sPair);


		/*Optional output, size of 1 PIP, f.i. 0.0001 for EUR/USD.
		Size of 1 pip in counter currency units; accessible with the PIP variable.About ~1 / 10000 of the asset price.
		The pip size is normally 0.0001 for assets(such as currency pairs) with a single digit price, 0.01 for assets with a price between 10 and 200 
		(such as USD / JPY and most stocks), and 1 for assets with a 4 - or 5 - digit price.For consistency, use the same pip sizes for all your asset lists.
		*/
		if (pPip)
		{
			const picojson::value& pair_decimals = objAssetPairVal.get("pair_decimals");
			char  decval[] = "0.0000000000";
			int ipair_decimals = atoi(pair_decimals.to_str().c_str());
			decval[ipair_decimals + 1] = '1';
			*pPip = atof(decval);
			logDetails = logDetails + "Pip: " + ftoa(*pPip) + " ";
		}


		/*Optional output, minimum order size, i.e.number of contracts for 1 lot of the asset.For currencies it's usually 10000 with mini lot accounts and
		1000 with micro lot accounts. For CFDs it's usually 1, but can also be a fraction of a contract, f.i. 0.1.
		https://support.kraken.com/hc/en-us/articles/205893708-What-is-the-minimum-order-size-
		Number of contracts for 1 lot of the asset; accessible with the LotAmount variable.
		It's the smallest amount that you can buy or sell without getting the order rejected or a "odd lot size" warning. 
		For currencies the lot size is normally 1000 on a micro lot account, 10000 on a mini lot account, and 100000 on standard lot accounts. 
		Some CFDs can have a lot size less than one contract, such as 0.1 contracts. For most other assets it's normally 1 contract per lot.
		*/			
		double minOrderSize = GetMinOrderSize(sPair);
		if (minOrderSize == 0)
			return 0;

		if (pMinAmount)
		{
			*pMinAmount = minOrderSize;//*pPip;
			double *jsonCached = SymbolLotAmount.TryGetValue(sAsset);
			if (!jsonCached)
				SymbolLotAmount.Add(sAsset, *pMinAmount);
			logDetails = logDetails + "MinAmount: " + ftoa(*pMinAmount) + " ";
		}


		/*  Optional output, cost of 1 PIP profit or loss per lot, in units of the account currency.
		Value of 1 pip profit or loss per lot, in units of the account currency. Accessible with the PipCost variable and internally used for calculating the trade profit.
		When the asset price rises or falls by x, the equivalent profit or loss of a trade in account currency is x * Lots * PIPCost / PIP. For assets with pip size 1 and
		one contract per lot, the pip cost is just the conversion factor from counter currency to account currency. For calculating it manually, multiply LotAmount with
		PIP and divide by the price of the account currency in the asset's counter currency. Example 1: AUD/USD on a micro lot EUR account has PipCost
		of 1000 * 0.0001 / 1.11 (current EUR/USD price) = 0.09 EUR. Example 2: AAPL stock on a USD account has PipCost of 1 * 0.01 / 1.0 = 0.01 USD = 1 cent.
		Example 3: S&P500 E-Mini futures on a USD account have PipCost of 50 USD (1 point price change of the underlying is equivalent to $50 profit/loss of an
		S&P500 E-Mini contract).

		if account curr==basecurrency   1 * *pPip / 1
		if account curr==ZUSD   1 * *pPip / XBT/USD
		*/
		if (pPipCost)
		{
			*pPipCost = (minOrderSize * *pPip) * g_BaseCurrAccCurrConvRate;
			logDetails = logDetails + "PipCost: " + ftoa(*pPipCost) + " ";
		}

		/*  pMarginCost
		Optional output, initial margin cost for buying 1 lot of the asset in units of the account currency. Alternatively, the leverage of the asset when
		negative (f.i. -50 for 50:1 leverage). If not supported, calculate it as decribed under asset list.

		Initial margin for purchasing 1 lot of the asset in units of the account currency. Depends on account leverage, account currency, and counter currency;
		accessible with the MarginCost variable. Internally used for the conversion from trade Margin to Lot amount: the number of lots that can be purchased with
		a given trade margin is Margin / MarginCost. Also affects the Required Capital and the Annual Return in the performance report.
		Can be left at 0 when Leverage (see below) is used for determining the margin.

		MarginCost = Asset price / Leverage * PipCost / PIP

		*/
		if (pMargin)
			*pMargin = 0;

		Log(logDetails, 0, false);

	}
	catch (const std::exception& e)
	{
		Log(e.what(),1,true);
		Log("BrokerAsset OUT", 0,false);
		return 0;
	}




	//return 1 if successfull
	Log("BrokerAsset OUT", 0,false);
	return 1;

}

/////////////////////////////////////////////////////////////////////
DLLFUNC int BrokerTime(DATE *pTimeGMT)
{

	//TODO remove
	//return 2;

	Log("BrokerTime IN", 0,false);

	int ret = 0;
	std::string ApiPath = "/0/public/Time";

	try
	{

		picojson::value::object objJson;
		if (!CallAPIAndParse(ApiPath, objJson, "", "", true))
		{
			Log("Error calling API BrokerTime", 1,true);
			Log("BrokerTime OUT", 0,false);
			return 0;
		}

		picojson::value::object::const_iterator iA = objJson.begin(); ++iA;//iA shoud be Time Series Object
		std::string sTickTime = iA->second.get("unixtime").to_str().c_str();
		COleDateTime tTickTime;
		tTickTime = convertTime((__time32_t)(atol(sTickTime.c_str())));

		COleDateTime odtStart(tTickTime);
		CString sStart = odtStart.Format(_T("%A, %B %d, %Y %H:%M:%S"));

		pTimeGMT = &tTickTime.m_dt;

	}
	catch (const std::exception& e)
	{
		Log(e.what(),1,true);
		Log("BrokerTime OUT", 0,false);
		return 0;
	}

	/*
	{"error":[],"result":{"unixtime":1497422201,"rfc1123":"Wed, 14 Jun 17 06:36:41 +0000"}}

	0 when the connection to the server was lost, and a new login is required.
	1 when the connection is ok, but the market is closed or trade orders are not accepted.
	2 when the connection is ok and the market is open for trading at least one of the subscribed assets.
	*/
	Log("BrokerTime OUT", 0,false);
	return 2;
}


/*
Optional function. Returns the current account status, or changes the account if multiple accounts are supported. Called repeatedly during the trading session. 
If the BrokerAccount function is not provided, f.i. when using a FIX API, Zorro calculates balance, equity, and total margin itself.
Parameters:
Account Input, new account number or NULL for using the current account.
pBalance Optional output, current balance on the account.
pTradeVal Optional output, current value of all open trades; the difference between account equity and balance. 
If not available, it can be replaced by a Zorro estimate with the SET_PATCH broker command.
pMarginVal Optional output, current total margin bound by all open trades.
*/

DLLFUNC int BrokerAccount(char* Account, double *pdBalance, double *pdTradeVal, double *pdMarginVal)
{
	Log("BrokerAccount IN", 0,false);

	
	std::string ApiPath = "/0/private/Balance";
	//std::string ApiPath = "/0/private/TradeBalance";
	std::string data = "aclass=currency&asset=" + g_BaseTradeCurrency;

	try
	{

		picojson::value::object objJson;
		if (!CallAPIAndParse(ApiPath, objJson, "Account", data, true))
		{
			Log("Error calling API BrokerAccount", 1,true);
			Log("BrokerAccount OUT", 0, false);
			return 0;
		}

		picojson::value::object::const_iterator iA = objJson.begin(); ++iA;//iA shoud be Time Series Object
		std::string sBal = iA->second.get(g_BaseTradeCurrency).to_str().c_str();
		if (sBal == "null")
			sBal = iA->second.get("X" + g_BaseTradeCurrency).to_str().c_str();
		if (sBal == "null")
			sBal = iA->second.get("Z" + g_BaseTradeCurrency).to_str().c_str();
		if (sBal == "null")
		{
			Log("BaseCurrency Not Found for AccountBalance BrokerAccount", 1, true);
			Log("BrokerAccount OUT", 0, false);
			return 0;
		}

		if (pdBalance)
			*pdBalance = atof(sBal.c_str());

		if (g_BaseTradeCurrency != g_DisplayCurrency && pdBalance)
			*pdBalance = *pdBalance * g_BaseCurrAccCurrConvRate;



		//*pdBalance = atof(iA->second.get("eb").to_str().c_str()); //eb = equivalent balance (combined balance of all currencies)
		//*pdTradeVal = *pdBalance- atof(iA->second.get("tb").to_str().c_str()) ;//tb = trade balance (combined balance of all equity currencies) - eb
		//*pdMarginVal = atof(iA->second.get("ml").to_str().c_str());//ml = margin level = (equity / initial margin) * 100

	}
	catch (const std::exception& e)
	{
		Log(e.what(),1,true);
		Log("BrokerAccount OUT", 0,false);
		return 0;
	}

	Log("BrokerAccount OUT", 0,false);
	return 1;
}


/*
Enters a long or short trade at market. Also used for NFA compliant accounts to close a trade by opening a position in the opposite direction.
Asset Input, name of the asset, f.i. "EUR/USD". Some broker APIs don't accept a "/" slash in an asset name; the plugin must remove the slash in that case.
nAmount Input, number of contracts, positive for a long trade and negative for a short trade. The number of contracts is the number of lots multiplied with the LotAmount.
If LotAmount is < 1 (f.i. for a CFD with 0.1 contracts lot size), the number of lots is given here instead of the number of contracts.
dStopDist Input, 'safety net' stop loss distance to the opening price, or 0 for no stop. This is not the real stop loss, which is handled by the trade engine. 
Placing the stop is not mandatory. NFA compliant orders do not support a stop loss; 
in that case dStopDist is 0 for opening a trade and -1 for closing a trade by opening a position in opposite direction.
pPrice Optional output, the current asset price at which the trade was opened.
Returns:
Trade ID number when opening a trade, or 1 when buying in opposite direction for closing a trade the NFA compliant way, or 0 when the trade could not be opened or closed. 
If the broker API does not deliver a trade ID number (for instance with NFA brokers that do not store individual trades), 
the plugin can just return an arbitrary unique number f.i. from a counter.
*/
DLLFUNC int BrokerBuy(char* Asset, int nAmount, double dStopDist, double *pPrice)
{
	Log("BrokerBuy IN", 0,false);
	int ret = 0;

	if (!IsLoggedIn())
		return 0;

	std::string sAsset = GetAsset(Asset);
	Log("BrokerBuy Params: " + sAsset + " Amount:" + itoa(nAmount) + " StopDist:" + ftoa(dStopDist),0,false);

	std::string ApiPath = "/0/private/AddOrder";

	if (nAmount == 0) return 0;
	std::string orderType;
	if (dStopDist == 0)
		orderType = nAmount > 0 ? "buy" : "sell";
	else if (dStopDist == -1)
		orderType = nAmount < 0 ? "sell" : "buy";
	else
	{
		Log("Error calling API BrokerBuy - Type undefined NFA Flag set ?", 1, true);
		Log("BrokerBuy OUT", 0, false);
		return 0;
	}
	nAmount = abs(nAmount);

	//double *jsonCached = SymbolLotAmount.TryGetValue(sAsset);
	//if (!jsonCached)
	//{
	//	BrokerError("Error calling API BrokerBuy - Symbol not cached fro LotAmount");
	//	Log("Error calling API BrokerBuy - Symbol not cached fro LotAmount", 1);
	//	Log("BrokerBuy OUT", 0);
	//	return 0;
	//}

	double famount = nAmount;// **jsonCached;
	double minOrderSize = GetMinOrderSize(sAsset);
	if (minOrderSize == 0)
		return 0;
	if (GetMinOrderSize(sAsset) < 1)
		famount = nAmount* GetMinOrderSize(sAsset);

	std::string sAmount = ftoa(famount);
	std::string tradeId = GetTradeIDstring();

	//TODO Swap lines
	std::string data = "pair=" + sAsset + "&type=" + orderType + "&ordertype=market&volume=" + sAmount + "&leverage=none&userref=" + tradeId;
	//std::string data = "nonce=" + nonce + "&pair=" + sAsset + "&type=" + orderType + "&ordertype=limit&volume=" + sAmount + "&leverage=none&price=0.00000010&userref=" + tradeId;
	if (g_DemoTrading)
	{
		data = data + "&validate=true";

		double *jsonCached = DemoTradesID.TryGetValue(atoi(tradeId.c_str()));
		if (!jsonCached)
			DemoTradesID.Add(atoi(tradeId.c_str()), famount);

	}


	std::string content;// = "{\"error\":[],\"result\":{\"descr\":{\"order\":\"buy 3000.00000000 XDGXBT @ limit 0.00000010\"},\"txid\":[\"OP24GE-Y7TL4-SDSXQE\"]}}";

	try
	{
		Log(data, 0, false);
		picojson::value::object objJson;
		if (!CallAPIAndParse(ApiPath, objJson, sAsset, data, false))
		{
			Log("Error calling API BrokerBuy", 1,true);
			Log("BrokerBuy OUT", 0,false);
			return 0;
		}

		picojson::value::object::const_iterator iA = objJson.begin(); ++iA;//iA shoud be Time Series Object
		std::string descr = iA->second.get("descr").serialize();
		std::string txid = iA->second.get("txid").serialize();
		if (txid.length() > 8 || g_DemoTrading)
		{
			double *lastPrice = SymbolLastPrice.TryGetValue(sAsset);
			Log(descr + " - " + tradeId + " - " + ftoa(*lastPrice), 0, true);
		} 

		if (txid == "null" && !g_DemoTrading)
		{
			Log("Error calling API BrokerBuy", 1, true);
			Log("BrokerBuy OUT", 0, false);
			return 0;
		}


	}
	catch (const std::exception& e)
	{
		Log(e.what(),1,true);
		Log("BrokerBuy OUT", 0,false);
		return 0;
	}


	Log("BrokerBuy OUT", 0,false);
	if (dStopDist == -1)
		return 1;
	else
		return atoi(tradeId.c_str());

}

// returns negative amount when the trade was closed
DLLFUNC int BrokerTrade(int nTradeID, double *pOpen, double *pClose, double *pRoll, double *pProfit)
{
	Log("BrokerTrade IN", 0,false);

	if (!IsLoggedIn())
		return 0;

	//TODO
	//nTradeID = 1502429247;
	if (g_DemoTrading)
	{
		//double *DemoTrade = DemoTradesID.TryGetValue(nTradeID);
		return 0;//*DemoTrade;
	}

	int ret = 0;
	std::string ApiPath = "/0/private/ClosedOrders";//QueryOrders"

	char cTradeID[20];
	_itoa(nTradeID, cTradeID, 10);
	std::string sTradeID(cTradeID);
	std::string data = "trades=true&userref=" + sTradeID;
	std::string logDetails = "TradeID Details: " + sTradeID + " ";

	try
	{
		//TODO
		//std::string content;// = "{\"error\":[],\"result\":{\"closed\":{\"OQLOGW-HS7AR-PX3HJR\":{\"refid\":null,\"userref\":1502429247,\"status\":\"closed\",\"reason\":\"User canceled\",\"opentm\":1502421611.0928,\"closetm\":1502421767.3221,\"starttm\":0,\"expiretm\":0,\"descr\":{\"pair\":\"XDGXBT\",\"type\":\"buy\",\"ordertype\":\"limit\",\"price\":\"0.00000010\",\"price2\":\"0\",\"leverage\":\"none\",\"order\":\"buy 3000.00000000 XDGXBT @ limit 0.00000010\"},\"vol\":\"3000.00000000\",\"vol_exec\":\"3000.00000000\",\"cost\":\"0.00168000\",\"fee\":\"0.00000436\",\"price\":\"0.00000056\",\"misc\":\"\",\"oflags\":\"fciq\"}},\"count\":1}}";
		Log(data, 0,false);
		picojson::value::object objJson;
		if (!CallAPIAndParse(ApiPath, objJson, "", data, true))
		{
			Log("Error calling API BrokerTrade", 1,true);
			Log("BrokerTrade OUT", 0,false);
			return 0;
		}

		picojson::value::object::const_iterator iA = objJson.begin(); ++iA;
		Log(iA->second.serialize(), 0,false);
		const picojson::value::object& firstBlock = iA->second.get<picojson::object>().begin()->second.get<picojson::object>();
		const picojson::value::object& descrBlock = firstBlock.begin()->second.get("descr").get<picojson::object>();
		std::string pair = descrBlock.at("pair").to_str();
		logDetails = logDetails + "Pair: " + pair + " ";

		if (pOpen)
		{
			*pOpen = atof(firstBlock.begin()->second.get("price").to_str().c_str());
			logDetails = logDetails + "Open: " + ftoa(*pOpen) + " ";
		}

		std::string type = descrBlock.at("type").to_str();
		logDetails = logDetails + "Type: " + type + " ";

		double *lastPrice = SymbolLastPrice.TryGetValue(pair);
		if (pClose && lastPrice)
		{
			*pClose = *lastPrice;
			logDetails = logDetails + "Close: " + ftoa(*pClose) + " ";
		}

		double execAmount = atof(firstBlock.begin()->second.get("vol_exec").to_str().c_str());
		ret = execAmount / GetMinOrderSize(pair);
		logDetails = logDetails + "Vol: " + ftoa(execAmount) + " ";

		double fee = atof(firstBlock.begin()->second.get("fee").to_str().c_str());
		logDetails = logDetails + "Fee: " + ftoa(fee) + " ";

		if (pProfit)
		{
			if (type=="buy")
				*pProfit = (*pClose * execAmount) - (*pOpen * execAmount) - fee;
			else if (type=="sell")
				*pProfit = (*pOpen * execAmount) - (*pClose * execAmount) - fee;

			logDetails = logDetails + "Profit " + g_BaseTradeCurrency + ": " + ftoa(*pProfit) + " ";
			if (g_BaseTradeCurrency!=g_DisplayCurrency)
				logDetails = logDetails + "Profit " + g_DisplayCurrency + ": " + ftoa(*pProfit * g_BaseCurrAccCurrConvRate) + " ";
		}

		*pRoll = 0;
		
		Log(logDetails, 0, true);

	}
	catch (const std::exception& e)
	{
		Log(e.what(),1,true);
		Log("BrokerTrade OUT", 0,false);
		return 0;
	}


	Log("BrokerTrade OUT", 0, false);
	return ret;
}

//DLLFUNC int BrokerSell(int nTradeID, int nAmount)
//{
//	Log("BrokerSell IN", 0);
//	if (!IsLoggedIn())
//		return 0;
//
//	int ret = 0;
//
//	Log("BrokerSell OUT", 0);
//	return ret;
//}

// 0 = test, 1 = relogin, 2 = login, -1 = logout
DLLFUNC int BrokerLogin(char* User, char* Pwd, char* Type, char* Account)
{
	Log("BrokerLogin IN", 0, false);



#ifdef FAKELOGIN
	return 1;
#endif

	if (User == NULL)
	{
		g_nLoggedIn = 0;
		ClearCache();
		return 0;
	}


	if (g_nLoggedIn == 0)
	{

		g_PrivateKey = ReadZorroIniConfig(std::string("PrivateKey"));
		g_PublicKey = ReadZorroIniConfig(std::string("PublicKey"));
		g_IntervalSec = atoi(ReadZorroIniConfig(std::string("IntervalSec")).c_str());
		g_BaseTradeCurrency = ReadZorroIniConfig(std::string("BaseTradeCurrency"));
		g_DisplayCurrency = ReadZorroIniConfig(std::string("DisplayCurrency"));
		g_DemoTrading = atoi(ReadZorroIniConfig(std::string("DemoTrading")).c_str());
		g_EnableLog = atoi(ReadZorroIniConfig(std::string("EnableLog")).c_str());
		g_UseCryptoCompareHistory = atoi(ReadZorroIniConfig(std::string("UseCryptoCompareHistory")).c_str());
		g_MinOrderSize = ReadZorroIniConfig(std::string("MinOrderSize"));

		////////////////// TEST HEADER
		//std::string ApiPath = "/0/private/TradeBalance";
		//std::string data = "aclass=currency&asset=USD";
		//std::string nonce = "636394818817728579";  //GetNonce();
		//data = "nonce=" + nonce + "&" + data;
		//std::string header = "";

		//std::string header2 = "{Content-Type: application/x-www-form-urlencoded\n\rAPI - Key: egYVR4ftMKt18mOIYe6grh3/1t+yTrj6ZG8V0dSDP001WCMHfrrRcyOQ\n\rAPI-Sign: xJFjPX1R3SeLkwlV6Bw2+waaB8PEkHvGqPaJSBS25LY6MpmAMfus6wpcD5GNyyT1pFQz5bR4C+QWWANLMi/NjQ==}";

		//for (int i = 0; i < 100; i++)
		//{
		//	header = GetHeader(ApiPath, data, nonce);
		//	header.erase(std::remove(header.begin(), header.end(), '\n'), header.end());
		//	header.erase(std::remove(header.begin(), header.end(), '\r'), header.end());
		//	header.erase(std::remove(header.begin(), header.end(), ' '), header.end());
		//	header2.erase(std::remove(header2.begin(), header2.end(), '\n'), header2.end());
		//	header2.erase(std::remove(header2.begin(), header2.end(), '\r'), header2.end());
		//	header2.erase(std::remove(header2.begin(), header2.end(), ' '), header2.end());
		//	Log("HEADER: " + header, 0, false);
		//	Log("HEADER2: " + header2, 0, false);
		//	if (header != header2)
		//		break;
		//}
		///////////////////

		if (g_BaseTradeCurrency != g_DisplayCurrency)
		{
			g_BaseCurrAccCurrConvRate = GetAccBaseCurrExchRate(g_BaseTradeCurrency + g_DisplayCurrency);
			if (g_BaseCurrAccCurrConvRate == 0)
			{
				Log("BrokerLogin OUT", 0, false);
				return 0;
			}
		}
		else
			g_BaseCurrAccCurrConvRate = 1;

		double val1 = 0;
		double val2 = 0;
		double val3 = 0;
		char account[20];
		ClearCache();
		g_nLoggedIn = BrokerAccount(account, &val1, &val2, &val3);
		if (g_nLoggedIn == 0)
		{
			Log("Can't Login - Retry",1 ,true);
			Log("BrokerLogin OUT", 0, false);
		}



		//////////////////////TODO

		//double p1=0;
		//double p2=0;
		//double p3=0;
		//double p4=0;
		//BrokerTrade(1502429247, &p1, &p2, &p3, &p4);   //Order OQLOGW-HS7AR-PX3HJR
		///////////////////////
	}


	Log("BrokerLogin OUT", 0, false);
	return g_nLoggedIn;
}

DLLFUNC int BrokerOpen(char* Name, FARPROC fpError, FARPROC fpProgress)
{
	Log("BrokerOpen IN", 0, false);


	int ret = 2;

	(FARPROC&)BrokerError = fpError;
	(FARPROC&)BrokerProgress = fpProgress;

	strcpy(Name, "Kraken");

	Log("BrokerOpen OUT", 0, false);
	return ret;

}

//DLLFUNC int BrokerStop(int nTradeID, double dStop)
//{
//	Log("BrokerStop IN", 0,false);
//
//	if (!IsLoggedIn())
//		return 0;
//
//	int ret = 0;
//
//
//	Log("BrokerStop OUT", 0,false);
//	return ret;
//}

DLLFUNC double BrokerCommand(int nCommand, DWORD dwParameter)
{
	Log("BrokerCommand IN", 0, false);

	if (!IsLoggedIn())
		return 0;


	int ret = 0;

	Log("BrokerCommand OUT", 0, false);
	return ret;
}