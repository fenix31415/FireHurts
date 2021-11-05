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

void mull(NiPoint3& a, const NiPoint3& b)
{
	a.x *= b.x, a.y *= b.y, a.z *= b.z;
}

struct Rect
{
	NiPoint3 M, B, alpha;  // halfed bounds
	std::vector<NiPoint3> get_vertexes()
	{
		std::vector<NiPoint3> ans = { { 1, 1, 1 }, { 1, 1, -1 }, { 1, -1, 1 },
			{ 1, -1, -1 }, { -1, 1, 1 }, { -1, 1, -1 }, { -1, -1, 1 }, { -1, -1, -1 } };
		for (auto& i : ans) {
			mull(i, B);
			i += M;
		}
		return ans;
	}
	std::vector<NiPoint3> get_edges()
	{
		std::vector<NiPoint3> ans = { { 0, 0, 1 }, { 0, 0, -1 }, { 0, 1, 0 },
			{ 0, -1, 0 }, { 1, 0, 0 }, { -1, 0, 0 } };
		for (auto& i : ans) {
			mull(i, B);
			i += M;
		}
		return ans;
	}
};

std::unordered_map<uint32_t, STAT_info> data;

void rotate(NiPoint3& A, const NiPoint3 O, const NiPoint3 angle)
{
	float cosa = cos(angle.z);
	float sina = sin(angle.z);

	float cosb = cos(angle.x);
	float sinb = sin(angle.x);

	float cosc = cos(angle.y);
	float sinc = sin(angle.y);

	float Axx = cosa * cosb;
	float Axy = cosa * sinb * sinc - sina * cosc;
	float Axz = cosa * sinb * cosc + sina * sinc;

	float Ayx = sina * cosb;
	float Ayy = sina * sinb * sinc + cosa * cosc;
	float Ayz = sina * sinb * cosc - cosa * sinc;

	float Azx = -sinb;
	float Azy = cosb * sinc;
	float Azz = cosb * cosc;

	A -= O;

	float px = A.x;
	float py = A.y;
	float pz = A.z;

	A.x = Axx * px + Axy * py + Axz * pz;
	A.y = Ayx * px + Ayy * py + Ayz * pz;
	A.z = Azx * px + Azy * py + Azz * pz;

	A += O;
}

void rotate(Rect& r, const NiPoint3 O, const NiPoint3 angle)
{
	r.alpha += angle;
	rotate(r.M, O, angle);
}

bool edge_separate(const NiPoint3 O, const NiPoint3 M, const std::vector<NiPoint3>& verts)
{
	bool ans = true;
	for (auto& i : verts) {
		ans = ans && ((i - O) * (M - O) < 0);
	}
	return ans;
}

bool cannot_separate_(Rect p, Rect a)
{
	rotate(a, p.M, -p.alpha);
	rotate(p, p.M, -p.alpha);
	auto p_verts = p.get_vertexes();
	auto a_verts = a.get_vertexes();
	for (auto& i : a_verts) {
		rotate(i, a.M, a.alpha);
	}

	auto p_edges = p.get_edges();
	for (auto& i : p_edges) {
		if (edge_separate(i, p.M, a_verts))
			return false;
	}
	return true;
}

bool cannot_separate(const Rect& A, const Rect& B)
{
	return cannot_separate_(A, B) && cannot_separate_(B, A);
}

bool is_collides(RE::StaticFunctionTag*, RE::TESObjectREFR* p, RE::TESObjectREFR* a)
{
	auto id = a->GetBaseObject()->formID;
	if (id > 0x01000000)
		id = id & 0x00FFFFFF | 0x01000000;	// possible overlaps ye (sory)

	auto player_scale = get_scale(p);
	NiPoint3 A, B;
	auto i = data.find(id);
	if (i == data.end()) {
		// default 100x100x40, h=10
		const float W_DEFAULTD_IV2 = 50.0f, H_DEFAULT_DIV2 = 40.0f, z0_DEFAULT = 10.0f;
		A = { -W_DEFAULTD_IV2, -W_DEFAULTD_IV2, -H_DEFAULT_DIV2 + z0_DEFAULT };
		B = { W_DEFAULTD_IV2, W_DEFAULTD_IV2, H_DEFAULT_DIV2 + z0_DEFAULT };
	} else {
		float scale = get_scale(a);
		A = i->second.A * scale;
		B = i->second.B * scale;
	}

	const float DOWN_DIV2 = 5.0f;
	const NiPoint3 PLAYER_BOUNDS = { 16.0f, 10.0f, 90.0f + DOWN_DIV2 };
	return cannot_separate({ p->GetPosition() + NiPoint3{ 0, 0, PLAYER_BOUNDS.z - DOWN_DIV2 },
							   PLAYER_BOUNDS * player_scale, { 0, 0, p->GetAngleZ() } },
		{ a->GetPosition() + (A + B) * 0.5f, (B - A) * 0.5f, a->GetAngle() });
}

bool RegisterFuncs(RE::BSScript::IVirtualMachine* a_vm)
{
	a_vm->RegisterFunction("is_collides", "ab01fhQuest", is_collides);
	return true;
}

void register_data_entry(uint32_t id, float scale, NiPoint3 a_center,
	NiPoint3 coll_center, NiPoint3 coll_bounds)
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
	register_data_entry(0xD61B6, 1.000000f, { 1074.327393f, -2311.895996f, 8952.120117f }, { 1088.000000f, -2240.000000f, 8992.000000f }, { 896.1055f, 975.5161f, 137.2891f });
	register_data_entry(0x3BD2E, 1.000000f, { -700.000000f, -3500.000000f, 8960.000000f }, { -700.515991f, -3500.052490f, 8979.500000f }, { 45.6865f, 54.5220f, 45.0000f });
	register_data_entry(0xE4E23, 1.000000f, { -4060.600098f, -3000.004639f, 8967.696289f }, { -4065.899414f, -3000.029053f, 8973.438477f }, { 38.2832f, 30.0762f, 87.1602f });
	register_data_entry(0x35F49, 1.000000f, { -3500.000000f, -1900.000000f, 8960.000000f }, { -3500.080566f, -1901.848267f, 8994.674805f }, { 80.6660f, 95.2422f, 85.0020f });
	register_data_entry(0x58301, 1.000000f, { -2700.000000f, -2300.000000f, 8960.000000f }, { -2692.947266f, -2300.012695f, 9074.977539f }, { 65.6846f, 66.5117f, 58.0312f });
	register_data_entry(0x35F47, 1.000000f, { -3100.000000f, -3500.000000f, 8960.000000f }, { -3099.428467f, -3496.166992f, 8995.402344f }, { 90.1318f, 92.9639f, 104.8047f });
	register_data_entry(0x30B39, 1.000000f, { -4059.250488f, -2225.849609f, 8960.000000f }, { -4056.412598f, -2226.499512f, 9070.000000f }, { 46.8911f, 44.2217f, 248.0000f });
	register_data_entry(0x33DA9, 1.000000f, { -3500.000000f, -3100.000000f, 8960.000000f }, { -3503.598389f, -3101.285889f, 8975.587891f }, { 50.5786f, 52.0269f, 35.6484f });
	register_data_entry(0xEF77F, 1.000000f, { 2336.000000f, -2816.000000f, 8960.000000f }, { 2368.000000f, -2816.000000f, 9184.000000f }, { 894.4766f, 872.9912f, 442.0000f });
	register_data_entry(0xEF2D6, 1.000000f, { -2700.000000f, -2700.000000f, 8960.000000f }, { -2699.434326f, -2700.725098f, 8983.433594f }, { 44.7485f, 47.7456f, 53.4941f });
	register_data_entry(0x3103F, 1.000000f, { -4117.179199f, -3248.154053f, 9002.896484f }, { -4113.239258f, -3249.644775f, 9073.396484f }, { 66.5947f, 63.5605f, 173.0000f });
	register_data_entry(0xAA71C, 1.000000f, { -2700.000000f, -3500.000000f, 8960.000000f }, { -2697.453857f, -3500.710205f, 8982.713867f }, { 42.5693f, 47.7461f, 50.7461f });
	register_data_entry(0xD61BE, 1.000000f, { 3712.000000f, -2880.000000f, 8960.000000f }, { 3712.000000f, -2848.000000f, 9184.000000f }, { 913.7188f, 910.7720f, 442.0000f });
	register_data_entry(0x108D70, 1.000000f, { -3808.000000f, -2752.000000f, 9152.000000f }, { -3808.000977f, -2752.967529f, 9232.500000f }, { 84.1328f, 81.8286f, 515.0000f });
	register_data_entry(0x10ACC0, 1.000000f, { 1000.181641f, -3268.005371f, 8966.669922f }, { 1010.586670f, -3279.781982f, 9152.000000f }, { 430.9429f, 406.8643f, 507.5527f });
	register_data_entry(0xCBB23, 1.000000f, { -1100.000000f, -3500.000000f, 8960.000000f }, { -1101.362061f, -3506.513916f, 9008.813477f }, { 58.9697f, 102.1133f, 131.6250f });
	register_data_entry(0x106112, 1.000000f, { -3100.000000f, -2300.000000f, 8960.000000f }, { -3101.369629f, -2298.995361f, 8980.781250f }, { 54.3740f, 46.5376f, 57.6582f });
	register_data_entry(0x13B40, 1.000000f, { -1900.000000f, -2300.000000f, 8960.000000f }, { -1900.000000f, -2300.000000f, 8964.068359f }, { 128.0000f, 128.0000f, 78.3965f });
	register_data_entry(0x2056204, 1.000000f, { -3837.281006f, -1820.753174f, 8960.000000f }, { -3840.016357f, -1813.958862f, 8983.044922f }, { 136.7603f, 135.9021f, 46.0938f });
	register_data_entry(0xFFF43, 1.000000f, { -3776.000000f, -2464.000000f, 9152.000000f }, { -3776.063965f, -2468.324951f, 9790.592773f }, { 170.2119f, 168.7788f, 1653.1855f });
	register_data_entry(0x108D7A, 1.000000f, { -3801.363037f, -3366.636963f, 9164.958984f }, { -3796.145508f, -3371.360352f, 9134.458984f }, { 198.6094f, 193.2520f, 403.0000f });
	register_data_entry(0x10D820, 1.000000f, { -1500.000000f, -2300.000000f, 8960.000000f }, { -1500.964844f, -2292.408691f, 9014.920898f }, { 81.0825f, 93.4292f, 143.8379f });
	register_data_entry(0x16D4B, 1.000000f, { -4075.094238f, -1879.892090f, 8994.178711f }, { -4072.594238f, -1881.392090f, 9061.678711f }, { 45.0000f, 305.0000f, 179.0000f });
	register_data_entry(0x204F5EB, 1.000000f, { -3801.649170f, -2099.714844f, 9157.246094f }, { -3809.833496f, -2103.148437f, 9819.954102f }, { 162.0464f, 173.4106f, 1701.4219f });
	register_data_entry(0x4C5CF, 1.000000f, { -640.000000f, -2048.000000f, 8960.000000f }, { -640.342896f, -2063.147949f, 9152.000000f }, { 406.6594f, 394.2286f, 512.0000f });
	register_data_entry(0x101A51, 1.000000f, { -1100.000000f, -2300.000000f, 8960.000000f }, { -1101.738037f, -2302.032227f, 8996.372070f }, { 73.6294f, 94.3110f, 106.7441f });
	register_data_entry(0xFB9B0, 1.000000f, { -1500.000000f, -1900.000000f, 8960.000000f }, { -1497.493530f, -1901.949829f, 9010.182617f }, { 91.3323f, 75.5286f, 134.3652f });
	register_data_entry(0x33DA4, 1.000000f, { -3500.000000f, -3500.000000f, 8960.000000f }, { -3498.902100f, -3499.757080f, 8969.000000f }, { 49.9297f, 54.9707f, 37.5605f });
	register_data_entry(0x4318B, 1.000000f, { -3500.000000f, -2700.000000f, 8960.000000f }, { -3501.099609f, -2699.806885f, 8985.584961f }, { 53.6367f, 51.3276f, 65.8887f });
	register_data_entry(0x51577, 1.000000f, { 160.000000f, -2688.000000f, 8960.000000f }, { 162.318298f, -2714.887451f, 9158.245117f }, { 369.1462f, 412.4521f, 524.4902f });
	register_data_entry(0xF6304, 1.000000f, { -224.000000f, -3360.000000f, 8960.000000f }, { -214.414154f, -3371.850830f, 9156.726562f }, { 375.7857f, 398.2300f, 521.4531f });
	register_data_entry(0x13B42, 1.000000f, { -2700.000000f, -3100.000000f, 8960.000000f }, { -2700.000000f, -3100.000000f, 8971.653320f }, { 60.0000f, 60.0000f, 53.1289f });
	register_data_entry(0xC2BF1, 1.000000f, { -1900.000000f, -1900.000000f, 8960.000000f }, { -1897.976074f, -1905.308105f, 9008.771484f }, { 81.3799f, 82.3420f, 131.5449f });
	register_data_entry(0xE4E24, 1.000000f, { -4079.597656f, -2764.141113f, 8971.039062f }, { -4074.465576f, -2763.789795f, 8977.946289f }, { 35.5591f, 45.1616f, 57.7148f });
	register_data_entry(0x951D8, 1.000000f, { -1900.000000f, -2700.000000f, 8960.000000f }, { -1897.899902f, -2701.527588f, 9000.375977f }, { 75.3789f, 73.9917f, 114.7520f });
	register_data_entry(0x3103C, 1.000000f, { -4094.702393f, -3449.509033f, 8979.580078f }, { -4086.211914f, -3451.003662f, 9050.080078f }, { 63.2871f, 61.3018f, 173.0000f });
	register_data_entry(0xE4E22, 1.000000f, { -4025.400879f, -2452.458984f, 8982.800781f }, { -4052.473145f, -2453.298340f, 8983.797852f }, { 64.2539f, 60.6514f, 66.6309f });
	register_data_entry(0xF597F, 1.000000f, { -544.000000f, -2752.000000f, 8960.000000f }, { -563.194580f, -2763.395508f, 9152.000000f }, { 399.0237f, 354.6035f, 512.0000f });
	register_data_entry(0x1092E2, 1.000000f, { -3100.000000f, -1900.000000f, 8960.000000f }, { -3098.660889f, -1902.231567f, 8975.877930f }, { 48.2061f, 49.0273f, 36.8730f });
	register_data_entry(0xCD824, 1.000000f, { -3100.000000f, -3100.000000f, 8960.000000f }, { -3099.470459f, -3099.466064f, 8978.296875f }, { 47.5425f, 47.4248f, 36.5938f });
	register_data_entry(0xDB682, 1.000000f, { -2300.000000f, -2700.000000f, 8960.000000f }, { -2308.376953f, -2699.847168f, 8997.219727f }, { 86.3706f, 88.2397f, 108.4395f });
	register_data_entry(0xB6C08, 1.000000f, { -1100.000000f, -3100.000000f, 8960.000000f }, { -1095.754028f, -3102.672607f, 8998.653320f }, { 74.7122f, 90.1211f, 111.3066f });
	register_data_entry(0xCBB2F, 1.000000f, { -2300.000000f, -1900.000000f, 8960.000000f }, { -2297.821289f, -1900.941040f, 8996.069336f }, { 96.1733f, 82.6287f, 106.1387f });
	register_data_entry(0xC6918, 1.000000f, { -2300.000000f, -2300.000000f, 8960.000000f }, { -2290.415039f, -2302.946777f, 8987.950195f }, { 93.5225f, 86.2998f, 89.9004f });
	register_data_entry(0x35F4A, 1.000000f, { -3500.000000f, -2300.000000f, 8960.000000f }, { -3500.009521f, -2302.951660f, 8977.527344f }, { 78.8794f, 80.6802f, 70.2637f });
	register_data_entry(0xE7C9A, 1.000000f, { -1100.000000f, -2700.000000f, 8960.000000f }, { -1097.105103f, -2702.149170f, 9010.651367f }, { 81.0256f, 84.5479f, 135.3027f });
	register_data_entry(0x108D74, 1.000000f, { -3776.000000f, -3008.000000f, 9152.000000f }, { -3778.332764f, -3009.688477f, 9116.000000f }, { 148.2515f, 182.5703f, 414.0000f });
	register_data_entry(0x90CA4, 1.000000f, { -1900.000000f, -3100.000000f, 8960.000000f }, { -1900.667114f, -3103.451660f, 8993.419922f }, { 80.3894f, 82.1899f, 100.8359f });
	register_data_entry(0xCD823, 1.000000f, { -3100.000000f, -2700.000000f, 8960.000000f }, { -3099.657471f, -2700.752930f, 8988.335938f }, { 50.4790f, 48.8921f, 73.3281f });
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
