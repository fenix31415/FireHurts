#include <unordered_map>

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= "FireHurts.log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::warn);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	logger::info("FireHurts v1.0.0");

	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = "FireHurts";
	a_info->version = 1;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

constexpr REL::ID get_scale_ID(19238);
REL::Relocation<float __fastcall(RE::TESObjectREFR* a)> get_scale(get_scale_ID);

using RE::NiPoint3;

struct STAT_info
{
	NiPoint3 A, B;
};

std::unordered_map<uint32_t, STAT_info> data;

bool intersects_rectangles(const NiPoint3& A1, const NiPoint3& B1, const NiPoint3& A2, const NiPoint3& B2)
{
	if (B1.y < A2.y)
		return false;
	if (B2.y < A1.y)
		return false;
	if (B1.x < A2.x)
		return false;
	if (B2.x < A1.x)
		return false;
	if (B1.z < A2.z)
		return false;
	if (B2.z < A1.z)
		return false;

	return true;
}

bool is_collides(RE::StaticFunctionTag*, RE::TESObjectREFR* p, RE::TESObjectREFR* a)
{
	auto id = a->GetBaseObject()->formID;
	if (id > 0x01000000)
		id = id & 0x00FFFFFF | 0x01000000;  // possible overlaps ye (sory)

	const float PLAYER_WIDTH = 30.0f, PLAYER_HEIGHT = 180.0f;
	auto player_pos = p->GetPosition();
	auto player_scale = get_scale(p);
	NiPoint3 A, B;
	auto i = data.find(id);
	if (i == data.end()) {
		// default 70x70x40, h=10
		const float W_DEFAULTD_IV2 = 50.0f, H_DEFAULT_DIV2 = 40.0f, z0_DEFAULT = 10.0f;
		A = { -W_DEFAULTD_IV2, -W_DEFAULTD_IV2, -H_DEFAULT_DIV2 + z0_DEFAULT };
		B = { W_DEFAULTD_IV2, W_DEFAULTD_IV2, H_DEFAULT_DIV2 + z0_DEFAULT };
	} else {
		float scale = get_scale(a);
		A = i->second.A * scale;
		B = i->second.B * scale;
	}
	
	auto a_pos = a->GetPosition();
	return intersects_rectangles(
		player_pos - NiPoint3{ PLAYER_WIDTH / 2.0f, PLAYER_WIDTH / 2.0f, 10.0f } * player_scale,
		player_pos + NiPoint3{ PLAYER_WIDTH / 2.0f, PLAYER_WIDTH / 2.0f, PLAYER_HEIGHT } * player_scale,
		A + a_pos, B + a_pos);
}

bool RegisterFuncs(RE::BSScript::IVirtualMachine* a_vm)
{
	a_vm->RegisterFunction("is_collides", "MyClass", is_collides);
	return true;
}

void register_data_entry(uint32_t id, float scale, NiPoint3 a_center,
	NiPoint3 coll_center,NiPoint3 coll_bounds)
{
	coll_center = a_center + (coll_center - a_center) / scale;
	coll_bounds = coll_bounds / scale;
	NiPoint3 boundsdiv2 = coll_bounds / 2.0f;
	if (id > 0x01000000)
		id = id & 0x00FFFFFF | 0x01000000;	// possible overlaps ye (sory)
	data[id] = { coll_center - boundsdiv2 - a_center, coll_center + boundsdiv2 - a_center };
}

void init_data()
{
	register_data_entry(0x33DA4, 1.000000f, { -3500.000000f, -3500.000000f, 8960.000000f }, { -3503.762451f, -3497.590576f, 8969.000000f }, { 70.4019f, 70.0425f, 37.5605f });
	register_data_entry(0x33DA9, 1.000000f, { -3500.000000f, -3100.000000f, 8960.000000f }, { -3498.708984f, -3101.281738f, 8975.587891f }, { 60.3579f, 61.6997f, 35.6484f });
	register_data_entry(0x4318B, 1.000000f, { -3500.000000f, -2700.000000f, 8960.000000f }, { -3501.102051f, -2699.811768f, 8985.584961f }, { 65.5303f, 63.2324f, 65.8887f });
	register_data_entry(0x35F4A, 1.000000f, { -3500.000000f, -2300.000000f, 8960.000000f }, { -3500.009521f, -2302.951660f, 8977.527344f }, { 78.8794f, 80.6802f, 70.2637f });
	register_data_entry(0x35F49, 1.000000f, { -3500.000000f, -1900.000000f, 8960.000000f }, { -3499.975342f, -1909.212769f, 8994.674805f }, { 110.0908f, 109.9712f, 85.0020f });
	register_data_entry(0x1092E2, 1.000000f, { -3100.000000f, -1900.000000f, 8960.000000f }, { -3102.338867f, -1898.542969f, 8975.877930f }, { 70.2632f, 71.0859f, 36.8730f });
	register_data_entry(0x106112, 1.000000f, { -3100.000000f, -2300.000000f, 8960.000000f }, { -3101.387939f, -2298.907227f, 8980.781250f }, { 67.1157f, 71.8218f, 57.6582f });
	register_data_entry(0xCD823, 1.000000f, { -3100.000000f, -2700.000000f, 8960.000000f }, { -3099.668701f, -2700.751221f, 8988.335938f }, { 61.0952f, 59.5347f, 73.3281f });
	register_data_entry(0xCD824, 1.000000f, { -3100.000000f, -3100.000000f, 8960.000000f }, { -3099.500000f, -3099.500000f, 8978.296875f }, { 67.0000f, 67.0000f, 36.5938f });
	register_data_entry(0x35F47, 1.000000f, { -3100.000000f, -3500.000000f, 8960.000000f }, { -3093.961182f, -3507.001221f, 8995.402344f }, { 122.3760f, 114.6323f, 104.8047f });
	register_data_entry(0xAA71C, 1.000000f, { -2700.000000f, -3500.000000f, 8960.000000f }, { -2699.768555f, -3500.702637f, 8982.713867f }, { 65.6445f, 66.1963f, 50.7461f });
	register_data_entry(0x13B42, 1.000000f, { -2700.000000f, -3100.000000f, 8960.000000f }, { -2700.000000f, -3100.000000f, 8971.653320f }, { 60.0000f, 60.0000f, 53.1289f });
	register_data_entry(0xEF2D6, 1.000000f, { -2700.000000f, -2700.000000f, 8960.000000f }, { -2699.428223f, -2703.504150f, 8983.433594f }, { 67.0791f, 64.5337f, 53.4941f });
	register_data_entry(0x58301, 1.000000f, { -2700.000000f, -2300.000000f, 8960.000000f }, { -2692.947266f, -2300.012695f, 9074.977539f }, { 65.6846f, 66.5117f, 58.0312f });
	register_data_entry(0xD61B6, 1.000000f, { -2700.000000f, -1900.000000f, 8960.000000f }, { -2686.327393f, -1828.104004f, 8999.879883f }, { 896.1055f, 975.5161f, 137.2891f });
	register_data_entry(0xCBB2F, 1.000000f, { -2300.000000f, -1900.000000f, 8960.000000f }, { -2297.832275f, -1905.404175f, 8996.069336f }, { 132.5415f, 109.6421f, 106.1387f });
	register_data_entry(0xC6918, 1.000000f, { -2300.000000f, -2300.000000f, 8960.000000f }, { -2295.233887f, -2302.965576f, 8987.950195f }, { 122.4087f, 124.7935f, 89.9004f });
	register_data_entry(0xDB682, 1.000000f, { -2300.000000f, -2700.000000f, 8960.000000f }, { -2308.370605f, -2699.844238f, 8997.219727f }, { 106.2729f, 108.1528f, 108.4395f });
	register_data_entry(0xD61BE, 1.000000f, { -2300.000000f, -3100.000000f, 8960.000000f }, { -2261.426270f, -3087.444092f, 9192.000000f }, { 913.7188f, 910.7720f, 442.0000f });
	register_data_entry(0xEF77F, 1.000000f, { -2300.000000f, -3500.000000f, 8960.000000f }, { -2295.078613f, -3505.861084f, 9192.000000f }, { 894.4766f, 872.9912f, 442.0000f });
	register_data_entry(0x51577, 1.000000f, { -1900.000000f, -3500.000000f, 8960.000000f }, { -1898.271606f, -3507.964355f, 9158.245117f }, { 352.2122f, 323.1885f, 524.4902f });
	register_data_entry(0x90CA4, 1.000000f, { -1900.000000f, -3100.000000f, 8960.000000f }, { -1895.830566f, -3088.669434f, 8993.419922f }, { 109.8225f, 111.7544f, 100.8359f });
	register_data_entry(0x951D8, 1.000000f, { -1900.000000f, -2700.000000f, 8960.000000f }, { -1905.548096f, -2697.894531f, 9000.375977f }, { 122.9487f, 129.8086f, 114.7520f });
	register_data_entry(0x13B40, 1.000000f, { -1900.000000f, -2300.000000f, 8960.000000f }, { -1900.000000f, -2300.000000f, 8964.068359f }, { 128.0000f, 128.0000f, 78.3965f });
	register_data_entry(0xC2BF1, 1.000000f, { -1900.000000f, -1900.000000f, 8960.000000f }, { -1897.968018f, -1899.270874f, 9008.771484f }, { 129.6753f, 118.6338f, 131.5449f });
	register_data_entry(0xFB9B0, 1.000000f, { -1500.000000f, -1900.000000f, 8960.000000f }, { -1503.377319f, -1896.017822f, 9010.182617f }, { 127.0469f, 135.0854f, 134.3652f });
	register_data_entry(0x10D820, 1.000000f, { -1500.000000f, -2300.000000f, 8960.000000f }, { -1506.666870f, -2298.992920f, 9014.920898f }, { 118.8240f, 106.5977f, 143.8379f });
	register_data_entry(0xF597F, 1.000000f, { -1500.000000f, -2700.000000f, 8960.000000f }, { -1504.834351f, -2700.500000f, 9151.334961f }, { 411.4558f, 390.7300f, 510.6719f });
	register_data_entry(0x10ACC0, 1.000000f, { -1500.000000f, -3100.000000f, 8960.000000f }, { -1476.181763f, -3083.186035f, 9145.330078f }, { 403.7666f, 349.6001f, 507.5527f });
	register_data_entry(0xF6304, 1.000000f, { -1500.000000f, -3500.000000f, 8960.000000f }, { -1488.119507f, -3484.123779f, 9156.726562f }, { 390.9050f, 363.5776f, 521.4531f });
	register_data_entry(0xCBB23, 1.000000f, { -1100.000000f, -3500.000000f, 8960.000000f }, { -1097.492798f, -3498.556152f, 9008.813477f }, { 131.1423f, 118.0288f, 131.6250f });
	register_data_entry(0xB6C08, 1.000000f, { -1100.000000f, -3100.000000f, 8960.000000f }, { -1100.955200f, -3102.667480f, 8998.653320f }, { 105.8652f, 131.6562f, 111.3066f });
	register_data_entry(0xE7C9A, 1.000000f, { -1100.000000f, -2700.000000f, 8960.000000f }, { -1091.820068f, -2696.854004f, 9010.651367f }, { 155.1799f, 137.5786f, 135.3027f });
	register_data_entry(0x101A51, 1.000000f, { -1100.000000f, -2300.000000f, 8960.000000f }, { -1106.660522f, -2306.880371f, 8996.372070f }, { 122.3025f, 142.9990f, 106.7441f });
	register_data_entry(0x4C5CF, 1.000000f, { -1100.000000f, -1900.000000f, 8960.000000f }, { -1094.099731f, -1887.063721f, 9137.589844f }, { 396.5000f, 382.3423f, 483.1836f });
	register_data_entry(0x3BD2E, 1.000000f, { -700.000000f, -3500.000000f, 8960.000000f }, { -700.500000f, -3500.000000f, 8979.500000f }, { 75.0000f, 74.0000f, 45.0000f });
	register_data_entry(0x204F5EB, 1.000000f, { -3801.649170f, -2099.714844f, 9157.246094f }, { -3795.863281f, -2103.145508f, 9762.757812f }, { 217.9683f, 201.3525f, 1587.0254f });
	register_data_entry(0x2056204, 1.000000f, { -3837.281006f, -1820.753174f, 8960.000000f }, { -3840.016357f, -1813.958862f, 8983.044922f }, { 136.7603f, 135.9021f, 46.0938f });
	register_data_entry(0xFFF43, 1.000000f, { -3776.000000f, -2464.000000f, 9152.000000f }, { -3776.051025f, -2463.778320f, 9809.500000f }, { 232.5352f, 232.0859f, 1691.0000f });
	register_data_entry(0x108D70, 1.000000f, { -3808.000000f, -2752.000000f, 9152.000000f }, { -3808.000000f, -2752.000000f, 9232.500000f }, { 130.0000f, 126.0000f, 515.0000f });
	register_data_entry(0x108D74, 1.000000f, { -3776.000000f, -3008.000000f, 9152.000000f }, { -3767.389160f, -3009.569824f, 9116.000000f }, { 213.2241f, 225.6997f, 414.0000f });
	register_data_entry(0x108D7A, 1.000000f, { -3801.363037f, -3366.636963f, 9164.958984f }, { -3801.353027f, -3353.505615f, 9134.458984f }, { 205.0786f, 209.3403f, 403.0000f });
	register_data_entry(0x3103C, 1.000000f, { -4094.702393f, -3449.509033f, 8979.580078f }, { -4086.202148f, -3451.009033f, 9050.080078f }, { 82.9998f, 81.0000f, 173.0000f });
	register_data_entry(0x3103F, 1.000000f, { -4117.179199f, -3248.154053f, 9002.896484f }, { -4113.179199f, -3249.654053f, 9073.396484f }, { 84.0000f, 81.0000f, 173.0000f });
	register_data_entry(0xE4E23, 1.000000f, { -4060.600098f, -3000.004639f, 8967.696289f }, { -4081.600098f, -2999.514160f, 8973.438477f }, { 92.0000f, 99.4878f, 87.1602f });
	register_data_entry(0xE4E24, 1.000000f, { -4079.597656f, -2764.141113f, 8971.039062f }, { -4088.597656f, -2763.738037f, 8977.946289f }, { 92.0000f, 101.6021f, 57.7148f });
	register_data_entry(0xE4E22, 1.000000f, { -4025.400879f, -2452.458984f, 8982.800781f }, { -4070.400879f, -2452.986572f, 8983.797852f }, { 136.0000f, 131.8530f, 66.6309f });
	register_data_entry(0x30B39, 1.000000f, { -4059.250488f, -2225.849609f, 8960.000000f }, { -4054.250488f, -2227.349609f, 9070.000000f }, { 84.0000f, 81.0000f, 248.0000f });
	register_data_entry(0x16D4B, 1.000000f, { -4075.094238f, -1879.892090f, 8994.178711f }, { -4072.594238f, -1881.392090f, 9061.678711f }, { 45.0000f, 305.0000f, 179.0000f });
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);

	auto papyrus = SKSE::GetPapyrusInterface();
	if (!papyrus->Register(RegisterFuncs)) {
		return false;
	}

	init_data();
	
	return true;
}
