#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

/*
msm clock uses lowercase
qcom clock uses uppercase
*/

enum clk_types {
	CPU,
	DSI_PLL,
	GCC,
	GCC_MDSS,
	RPMCC,
	RPMCC_SMD
};

typedef struct clk_prefix {
	const enum clk_types clk_type;
	const char* clk_prefix;
	const bool clk_prefix_is_full_name;
} clk_prefix_t;

const clk_prefix_t kClkPrefixs[] = {
	// ref: msm/mdss/ *.c
	{DSI_PLL, "dsi0pll_", false},
	{DSI_PLL, "dsi1pll_", false},
	{DSI_PLL, "dsi_pll", false},

	// ref: dt-bindings/clock/msm-clocks-8952.h
	{GCC, "gcc_", false},
	{GCC, "blsp", false},

	// ref: msm/clock-gcc-8952.c msm_clocks_gcc_mdss_common[]
	{GCC_MDSS, "ext_pclk0_clk_src", true},
	{GCC_MDSS, "ext_byte0_clk_src", true},
	{GCC_MDSS, "byte0_clk_src", true},
	{GCC_MDSS, "pclk0_clk_src", true},
	{GCC_MDSS, "gcc_mdss_pclk0_clk", true},
	{GCC_MDSS, "gcc_mdss_byte0_clk", true},
	{GCC_MDSS, "mdss_mdp_vote_clk", true},
	{GCC_MDSS, "mdss_rotator_vote_clk", true},
	{GCC_MDSS, "ext_pclk1_clk_src", true},
	{GCC_MDSS, "ext_byte1_clk_src", true},
	{GCC_MDSS, "byte1_clk_src", true},
	{GCC_MDSS, "pclk1_clk_src", true},
	{GCC_MDSS, "gcc_mdss_pclk1_clk", true},
	{GCC_MDSS, "gcc_mdss_byte1_clk", true},

	// ref: msm/clock-cpu-8939.c
	{CPU, "a53ssmux_lc", true},
	{CPU, "a53ssmux_bc", true},
	{CPU, "a53ssmux_cci", true},
	{CPU, "a53_bc_clk", true},
	{CPU, "a53_lc_clk", true},
	{CPU, "cci_clk", true},

	// ref: qcom/clk-smd-rpm.c qm215_clks[]
	{RPMCC_SMD, "xo_clk_src", true},
	{RPMCC_SMD, "xo_a_clk_src", true},
	{RPMCC_SMD, "qdss_", false},
	{RPMCC_SMD, "pnoc_clk", true},
	{RPMCC_SMD, "pnoc_a_clk", true},
	{RPMCC_SMD, "snoc_clk", true},
	{RPMCC_SMD, "snoc_a_clk", true},
	{RPMCC_SMD, "bimc_clk", true},
	{RPMCC_SMD, "bimc_a_clk", true},
	{RPMCC_SMD, "bimc_gpu_clk", true},
	{RPMCC_SMD, "bimc_gpu_a_clk", true},
	{RPMCC_SMD, "ipa_", false},

	{RPMCC, "sysmmnoc_msmbus_", false},
	{RPMCC_SMD, "sysmmnoc_", false},

	{RPMCC_SMD, "bb_clk", false},
	{RPMCC_SMD, "rf_clk", false},
	{RPMCC_SMD, "div_", false},

	{RPMCC, "pnoc_", false},
	{RPMCC, "snoc_", false},
	{RPMCC, "bimc_", false},
	{RPMCC, "xo_", false},
	{RPMCC, "ln_bb_", false}, // Not found on QCOM driver
};

bool comparePrefix(string str, string prefix) {
	return !!(str.substr(0, prefix.length()) == prefix);
}

enum clk_types msmToQcom(string* msm, string* qcom) {
	enum clk_types actual_clk_type = GCC;
	*qcom = *msm;

	// Direct match
	if (*msm == "clk_a53_bc_clk") {
		*qcom = "APCS_MUX_C1_CLK";
		return CPU;
	} else if (*msm == "clk_a53_lc_clk") {
		*qcom = "APCS_MUX_C0_CLK";
		return CPU;
	} else if (*msm == "clk_cci_clk") {
		*qcom = "APCS_MUX_CCI_CLK";
		return CPU;
	} else if (*msm == "clk_div_clk1_a") {
		*qcom = "RPM_SMD_DIV_A_CLK1";
		return RPMCC_SMD;
	} else if (*msm == "clk_div_clk2_a") {
		*qcom = "RPM_SMD_DIV_A_CLK2";
		return RPMCC_SMD;
	}

	// Remove clk_ prefix
	qcom->erase(0, 4);

	// Judge clk type
	for (auto& [clk_type, clk_prefix, clk_prefix_is_full_name] : kClkPrefixs) {
		if (clk_prefix_is_full_name) {
			if (*qcom == clk_prefix) {
				actual_clk_type = clk_type;
			}
		} else {
			if (comparePrefix(*qcom, clk_prefix)) {
				actual_clk_type = clk_type;
			}
		}
	}

	// Convert to uppercase
	transform(qcom->begin(), qcom->end(), qcom->begin(), ::toupper);

	// Process specific clk types
	switch (actual_clk_type) {
		case RPMCC:
			// example: clk_xo_pil_pronto_clk => CXO_SMD_PIL_PRONTO_CLK
			if (comparePrefix(*qcom, "XO_")) {
				qcom->insert(0, "C");
				qcom->insert(3, "_SMD");
			}

			break;
		case RPMCC_SMD:
			// example: clk_xo_clk_src => RPM_SMD_XO_CLK_SRC
			qcom->insert(0, "RPM_SMD_");
			break;
		default:
			break;
	}

	return actual_clk_type;
}

int main(int argc, char* argv[])
{
	if (argc != 2)
		return 1;

	ifstream msmClkDtHeaderFile{string(argv[1])};
	string line;

	cout << "#ifndef _DTS_MSM8937_CLK_WRAPPER_H" << endl;
	cout << "#define _DTS_MSM8937_CLK_WRAPPER_H" << endl;
	cout << endl;

	while (getline(msmClkDtHeaderFile, line)) {
		// Only proceed for lines which we're interested in
		if (!comparePrefix(line, "#define "))
			continue;
		line.erase(0, strlen("#define "));
		if (comparePrefix(line, "__"))
			continue;

		// Extract msm clock name
		size_t delim_pos;
		delim_pos = line.find("\t");
		if (delim_pos == string::npos)
			delim_pos = line.find(" ");
		string msmClkName = line.substr(0, delim_pos);

		// Ignore GCC_* defines
		if (comparePrefix(msmClkName, "GCC_"))
			continue;

/*
		// for debug only
		cout << msmClkName << endl;
		continue;
*/

		// Generate qcom clock name
		string qcomClkName;
		enum clk_types clkType = msmToQcom(&msmClkName, &qcomClkName);

		// Output
		switch (clkType) {
			case CPU:
				cout << "/* CPU */" << endl;
				break;
				;;
			case DSI_PLL:
				cout << "/* DSI PLL */" << endl;
				break;
				;;
			case GCC:
				cout << "/* GCC */" << endl;
				break;
				;;
			case GCC_MDSS:
				cout << "/* GCC MDSS */" << endl;
				break;
				;;
			case RPMCC:
				cout << "/* RPMCC */" << endl;
				break;
				;;
			case RPMCC_SMD:
				cout << "/* RPMCC SMD */" << endl;
				break;
				;;
		}
		cout << "#if !defined(" << msmClkName << ")" << endl;
		cout << "#define " << msmClkName << " " << qcomClkName << endl;
		cout << "#elif !defined(" << qcomClkName << ")" << endl;
		cout << "#define " << qcomClkName << " " << msmClkName << endl;
		cout << "#endif" << endl << endl;
	}

	cout << "#endif // _DTS_MSM8937_CLK_WRAPPER_H" << endl;

	return 0;
}
