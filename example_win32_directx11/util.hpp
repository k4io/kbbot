#include "json.hpp"
#include "bdk.hpp"
#include <iostream>

class CUtil {
public:
	CUtil() { }
	~CUtil() { delete binance; }

	BAPI* binance;
	char m_cTextInput[64] = { "" };
	bool m_bIsTrading = false;
	bool m_bIsConnected = false;

	struct settings {
		char m_cApiKey[256] = { "" };
		char m_cSecretKey[256] = { "" };
		bool m_bAutosell = 0, m_bAutoBuy = 0, m_bChartTooltip = 0;
		int m_iBuyMode = 0, m_iTradingPair = 0, m_iChartInterval = 1;
		float m_fMinimumProfitMargin = 20.f;
		float m_fHighColor[4] = { .1f, .8f, .1f, 1.f }, m_fLowColor[4] = { .8f, .1f, .1f, 1.f };
	}; settings m_settings;

	void SaveSettings() {
		std::ofstream fout;
		fout.open("settings.json");

		if (fout.is_open()) {
			nlohmann::json jsonData;
			jsonData["m_bAutosell"] = m_settings.m_bAutosell;
			jsonData["m_bAutoBuy"] = m_settings.m_bAutoBuy;
			jsonData["m_iBuyMode"] = m_settings.m_iBuyMode;
			jsonData["m_iTradingPair"] = m_settings.m_iTradingPair;
			jsonData["m_fMinimumProfitMargin"] = m_settings.m_fMinimumProfitMargin;
			jsonData["m_bChartTooltip"] = m_settings.m_bChartTooltip;
			jsonData["m_iChartInterval"] = m_settings.m_iChartInterval;
			jsonData["m_cApiKey"] = m_settings.m_cApiKey;
			jsonData["m_cSecretKey"] = m_settings.m_cSecretKey;
			fout << jsonData.dump(4) << std::endl;
			fout.close();
		}
		else {
			std::cout << "Failed to save settings!" << std::endl;
		}
	}

	void LoadSettings() {
		if (!std::filesystem::exists("settings.json")) SaveSettings();

		std::ifstream fin;
		fin.open("settings.json");


		if (fin.is_open()) {
			settings loadedSettings;
			nlohmann::json jsonData = nlohmann::json::parse(fin);
			loadedSettings.m_bAutosell = jsonData["m_bAutosell"];
			loadedSettings.m_bAutoBuy = jsonData["m_bAutoBuy"];
			loadedSettings.m_iBuyMode = jsonData["m_iBuyMode"];
			loadedSettings.m_iTradingPair = jsonData["m_iTradingPair"];
			loadedSettings.m_iChartInterval = jsonData["m_iChartInterval"];
			loadedSettings.m_fMinimumProfitMargin = jsonData["m_fMinimumProfitMargin"];
			loadedSettings.m_bChartTooltip = jsonData["m_bChartTooltip"];
			strcpy(loadedSettings.m_cApiKey, jsonData["m_cApiKey"].get<std::string>().c_str());
			strcpy(loadedSettings.m_cSecretKey, jsonData["m_cSecretKey"].get<std::string>().c_str());
			fin.close();

			this->m_settings = loadedSettings;
		}
		else {
			std::cout << "Failed to load settings!" << std::endl;
		}
	}

	int CheckBinanceKeys() {
		if (strlen(this->m_settings.m_cApiKey) == 0 || strlen(this->m_settings.m_cSecretKey) == 0) {
			MessageBoxA(0, "Please enter your Binance API keys!", "Error", MB_OK | MB_ICONERROR);
			return 0;
		}

		const auto& tbinance = new BAPI(this->m_settings.m_cApiKey, this->m_settings.m_cSecretKey);
		auto accountInfo = tbinance->getAccountInfo();

		auto account_result = accountInfo["code"];

		if (account_result.is_null())
			return 1;
		else
			return account_result.get<int>();
	}

}; CUtil* g_pUtil = nullptr;