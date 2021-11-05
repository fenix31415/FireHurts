#include <iostream>
#include<string>
#include<vector>

struct uint2
{
	uint32_t x, y;
	uint2 operator-(const uint2& r) const
	{
		return {x-r.x, y-r.y};
	}
	uint2 operator+(const uint2& r) const
	{
		return { x + r.x, y + r.y };
	}
	bool operator==(const uint2& r) const {
		return x == r.x && y == r.y;
	}
	bool operator!=(const uint2& r) const
	{
		return x != r.x || y != r.y;
	}
	std::string to_string()
	{
		return "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
	}
};

struct uint4
{
	uint32_t x, y, z, w;
	uint4 operator-(const uint4& r) const
	{
		return { x - r.x, y - r.y, z - r.z, w - r.w };
	}
	uint4 operator+(const uint4& r) const
	{
		return { x + r.x, y + r.y, z + r.z, w + r.w };
	}
	std::string to_string()
	{
		return "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
	}
};

const int THREADS_PER_HASH = 8;
const int _PARALLEL_HASH = 4;
const int ACCESSES = 64;
uint4 d_dag(uint32_t a, uint32_t b)
{
	return { a * b, a + b, a - b, a ^ b };
}

int bfe23(uint32_t a)
{
	return a & 7;
}


const int d_dag_size = 1 << 10;
uint2 state[8][12];  // state[thread_num] = state[12]

uint4 mix[8][4];
uint32_t offset[8][4];
uint32_t init0[8][4];

uint2 shuffle[8][8];
uint2 shuffle1[8][4];
uint32_t thread_mix[8];

void init_(uint2* state, uint32_t nonce)
{
	nonce = (nonce * nonce) ^ nonce;
	for (uint32_t i = 0; i < 12; i++) {
		state[i] = { i * 31415 + nonce, i * 31415 + nonce * nonce };
	}
}

void init(uint2 state[8][12], uint32_t nonce)
{
	for (uint32_t t = 0; t < 8; t++) {
		init_(state[t], nonce + t);
	}
}

const uint32_t FNV_PRIME = 0x01000193u;
uint32_t fnv(uint32_t a, uint32_t b)
{
	return a * FNV_PRIME ^ b;
}

uint4 fnv4(uint4 a, uint4 b)
{
	uint4 c;
	c.x = a.x * FNV_PRIME ^ b.x;
	c.y = a.y * FNV_PRIME ^ b.y;
	c.z = a.z * FNV_PRIME ^ b.z;
	c.w = a.w * FNV_PRIME ^ b.w;
	return c;
}

uint32_t fnv_reduce(uint4 v)
{
	return fnv(fnv(fnv(v.x, v.y), v.z), v.w);
}

bool compute_hash(uint32_t nonce, uint2 mix_hash[8][4])
{
	init(state, nonce);

	// Threads work together in this phase in groups of 8.
	const int thread_id = 0;  // 0..7

	for (int i = 0; i < 8; i += 4) {
		// 0: mix = state[0..1] at 0..3 threads
		// 1: mix = state[2..3] at 0..3 threads
		// 4: mix = state[0..1] at 4..7 threads
		for (int p = 0; p < 4; p++) {
			// shuffle[0..8] = state[0..8] at (i+p)-th thread
			for (uint32_t t = 0; t < 8; t++) {
				for (int j = 0; j < 8; j++) {
					shuffle[t][j] = { state[i + p][j].x, state[i + p][j].y };
				}
			}
			for (uint32_t t = 0; t < 8; t++) {
				auto ind = (t & 3) * 2;
				mix[t][p] = { shuffle[t][ind].x, shuffle[t][ind].y,
					shuffle[t][ind + 1].x, shuffle[t][ind + 1].y };
			}
			// 0..3: mix[p] = shuffle[0..7]
			
			// init0[0..4] = state[0] at 0 thread
			for (uint32_t t = 0; t < 8; t++) {
				init0[t][p] = shuffle[0][0].x;
			}
		}

		for (uint32_t a = 0; a < 64; a += 4) {
			int tt = bfe23(a);

			for (uint32_t b = 0; b < 4; b++) {
				for (int p = 0; p < 4; p++) {
					// offset[0..3] = f(init0, a, b, mix)
					for (uint32_t t = 0; t < 8; t++) {
						offset[t][p] = fnv(init0[t][p] ^ (a + b), ((uint32_t*)&mix[t][p])[b]) % d_dag_size;
					}
				}
				for (int p = 0; p < 4; p++) {
					for (uint32_t t = 0; t < 8; t++) {
						offset[t][p] = offset[tt][p];
					}
					for (uint32_t t = 0; t < 8; t++) {
						mix[t][p] = fnv4(mix[t][p], d_dag(offset[t][p], t));
					}
				}
			}
		}

		for (int p = 0; p < 4; p++) {
			// anses
			for (uint32_t t = 0; t < 8; t++) {
				thread_mix[t] = fnv_reduce(mix[t][p]);
			}

			// update mix across threads
			for (uint32_t t = 0; t < 8; t++) {
				shuffle1[t][0].x = thread_mix[0];
				shuffle1[t][0].y = thread_mix[1];
				shuffle1[t][1].x = thread_mix[2];
				shuffle1[t][1].y = thread_mix[3];
				shuffle1[t][2].x = thread_mix[4];
				shuffle1[t][2].y = thread_mix[5];
				shuffle1[t][3].x = thread_mix[6];
				shuffle1[t][3].y = thread_mix[7];
			}

			for (uint32_t t = 0; t < 8; t++) {
				if (i + p == t) {
					// move mix into state:
					state[t][8] = shuffle1[t][0];
					state[t][9] = shuffle1[t][1];
					state[t][10] = shuffle1[t][2];
					state[t][11] = shuffle1[t][3];
				}
			}
		}
	}

	for (uint32_t t = 0; t < 8; t++) {
		mix_hash[t][0] = state[t][8];
		mix_hash[t][1] = state[t][9];
		mix_hash[t][2] = state[t][10];
		mix_hash[t][3] = state[t][11];
	}
	return false;
}

const int BLOCKS = 16 * 16 / 8;
const int THREADS_PER_BLOCK = 8 * BLOCKS;
uint2 state_[THREADS_PER_BLOCK][12];

uint4 mix_[THREADS_PER_BLOCK][4];
uint32_t offset_[THREADS_PER_BLOCK][4];
uint32_t init0_[THREADS_PER_BLOCK][4];

uint2 shuffle_[THREADS_PER_BLOCK][8];
uint2 shuffle1_[THREADS_PER_BLOCK][4];
uint32_t thread_mix_[THREADS_PER_BLOCK];

void mul(double A[16][16]) {
	for (int i = 0; i < 16; i++) {
		for (int j = 0; j < 16; j++) {
			A[i][j] *= (double)FNV_PRIME;
		}
	}
}

bool compute_hash_(uint32_t nonce, uint2 mix_hash[THREADS_PER_BLOCK][4])
{
	for (uint32_t i = 0; i < BLOCKS; i++) {
		init(&state_[i * 8], nonce + i);
	}
	
	// const int thread_id = threadIdx.x & (THREADS_PER_BLOCK - 1);
	// const int mix_idx = thread_id & 3;
	// const group = thread_id & 0xFFFFFFF8

	for (int i = 0; i < 8; i += 4) {
		for (int p = 0; p < 4; p++) {
			for (int j = 0; j < 8; j++) {
				// shuffle[j].x = SHFL(state[j].x, i + p + group, THREADS_PER_BLOCK);
				// shuffle[j].y = SHFL(state[j].y, i + p + group, THREADS_PER_BLOCK);
				for (uint32_t t = 0; t < THREADS_PER_BLOCK; t++) {
					shuffle_[t][j] = { state_[(t & 0xFFFFFFF8) + i + p][j].x, state_[(t & 0xFFFFFFF8) + i + p][j].y };
				}
			}
			// same
			for (uint32_t t = 0; t < THREADS_PER_BLOCK; t++) {
				auto ind = (t & 3) * 2;
				mix_[t][p] = { shuffle_[t][ind].x, shuffle_[t][ind].y, shuffle_[t][ind + 1].x, shuffle_[t][ind + 1].y };
			}
			// init0[p] = SHFL(shuffle[0].x, group, THREADS_PER_BLOCK);
			for (uint32_t t = 0; t < THREADS_PER_BLOCK; t++) {
				init0_[t][p] = shuffle_[t & 0xFFFFFFF8][0].x;
			}
		}

		for (uint32_t a = 0; a < 64; a += 4) {
			int tt = bfe23(a);
			for (uint32_t b = 0; b < 4; b++) {
				for (int p = 0; p < 4; p++) {
					// same
					/*double A[16][16];
					for (int i = 0; i < 16; i++) {
						for (int j = 0; j < 16; j++) {
							A[i][j] = double(init0_[i * 8 + j][p] ^ (a + b));
						}
					}
					mul(A);
					for (int i = 0; i < 16; i++) {
						for (int j = 0; j < 16; j++) {
							offset_[i * 8 + j][p] = (uint32_t)((uint64_t) A[i][j]);
						}
					}
					for (uint32_t t = 0; t < THREADS_PER_BLOCK; t++) {
						offset_[t][p] ^= ((uint32_t*)&mix_[t][p])[b];
						offset_[t][p] = offset_[t][p] % d_dag_size;
					}*/
					double A[16][16];
					for (uint32_t t = 0; t < THREADS_PER_BLOCK; t++) {
						A[t >> 4][t & 15] = double(init0_[t][p] ^ (a + b));
					}
					for (uint32_t t = 0; t < THREADS_PER_BLOCK; t++) {
						A[t >> 4][t & 15] *= double(FNV_PRIME);
					}
					for (uint32_t t = 0; t < THREADS_PER_BLOCK; t++) {
						//offset_[t][p] = (init0_[t][p] ^ (a + b)) * FNV_PRIME;
						offset_[t][p] = A[t >> 4][t & 15];
						uint32_t tmp = (init0_[t][p] ^ (a + b)) * FNV_PRIME;
						if (offset_[t][p] != tmp) {  // <- here we have errors
							std::cout << (init0_[t][p] ^ (a + b)) << "\n";
						}
					}
					for (uint32_t t = 0; t < THREADS_PER_BLOCK; t++) {
						offset_[t][p] = (offset_[t][p] ^ ((uint32_t*)&mix_[t][p])[b]) % d_dag_size;
					}
					//for (uint32_t t = 0; t < THREADS_PER_BLOCK; t++) {
					//	offset_[t][p] = fnv(init0_[t][p] ^ (a + b), ((uint32_t*)&mix_[t][p])[b]) % d_dag_size;
					//}

					// offset[p] = SHFL(offset[p], group + t, THREADS_PER_BLOCK);
					for (uint32_t t = 0; t < THREADS_PER_BLOCK; t++) {
						offset_[t][p] = offset_[(t & 0xFFFFFFF8) + tt][p];
					}

					// same
					for (uint32_t t = 0; t < THREADS_PER_BLOCK; t++) {
						mix_[t][p] = fnv4(mix_[t][p], d_dag(offset_[t][p], t & 7));
					}
				}
			}
		}

		for (int p = 0; p < 4; p++) {
			// same
			for (uint32_t t = 0; t < THREADS_PER_BLOCK; t++) {
				thread_mix_[t] = fnv_reduce(mix_[t][p]);
			}
			// shuffle[0].x = SHFL(thread_mix, group + 0, THREADS_PER_BLOCK);
			// and so on
			for (uint32_t t = 0; t < THREADS_PER_BLOCK; t++) {
				shuffle1_[t][0].x = thread_mix_[(t & 0xFFFFFFF8) + 0];
				shuffle1_[t][0].y = thread_mix_[(t & 0xFFFFFFF8) + 1];
				shuffle1_[t][1].x = thread_mix_[(t & 0xFFFFFFF8) + 2];
				shuffle1_[t][1].y = thread_mix_[(t & 0xFFFFFFF8) + 3];
				shuffle1_[t][2].x = thread_mix_[(t & 0xFFFFFFF8) + 4];
				shuffle1_[t][2].y = thread_mix_[(t & 0xFFFFFFF8) + 5];
				shuffle1_[t][3].x = thread_mix_[(t & 0xFFFFFFF8) + 6];
				shuffle1_[t][3].y = thread_mix_[(t & 0xFFFFFFF8) + 7];
			}
			// if (i+p) == thread_id
			// state[8] = shuffle[0];
			// and so on
			for (uint32_t t = 0; t < THREADS_PER_BLOCK; t++) {
				if (i + p == (t & 7)) {
					state_[t][8] = shuffle1_[t][0];
					state_[t][9] = shuffle1_[t][1];
					state_[t][10] = shuffle1_[t][2];
					state_[t][11] = shuffle1_[t][3];
				}
			}
		}
	}
	
	// mix_hash[0] = state[8];
	// and so on
	for (uint32_t t = 0; t < THREADS_PER_BLOCK; t++) {
		mix_hash[t][0] = state_[t][8];
		mix_hash[t][1] = state_[t][9];
		mix_hash[t][2] = state_[t][10];
		mix_hash[t][3] = state_[t][11];
	}
	return false;
}

uint2 rigans[BLOCKS][8][4];
uint2 myans[THREADS_PER_BLOCK][4];
void test(const uint32_t SEED)
{
	const int cur_blocks = BLOCKS;

	for (uint32_t ans_i = 0; ans_i < BLOCKS; ans_i++) {
		compute_hash(SEED + ans_i, rigans[ans_i]);
	}
	//compute_hash_(SEED, myans);
	compute_hash_(SEED, myans);

	for (uint32_t ans_i = 0; ans_i < cur_blocks; ans_i++) {
		for (uint32_t i = 0; i < 8; i++) {
			for (uint32_t j = 0; j < 4; j++) {
				if (rigans[ans_i][i][j] != myans[ans_i * 8 + i][j]) {
					for (uint32_t z = 0; z < cur_blocks; z++) {
						for (uint32_t x = 0; x < 8; x++) {
							for (uint32_t y = 0; y < 4; y++) {
								std::cout << rigans[z][x][y].to_string() << " ";
								if (myans[z * 8 + x][y] != rigans[z][x][y])
									std::cout << "!= ";
								std::cout << myans[z * 8 + x][y].to_string() << "\n";
							}
							std::cout << "\n";
						}
						std::cout << "--------------------------------------------\n";
					}
					return;
				}
			}
		}
	}

	std::cout << "OK\n";
}

void test_all()
{
	test(3);
	test(31);
	test(314);
	test(3141);
	test(31415);
}

uint32_t foo(uint32_t a)
{
	return a * FNV_PRIME;
}

uint64_t foo0(uint64_t a)
{
	return a * FNV_PRIME;
}

double mull(double a)
{
	return a * double(FNV_PRIME);
}

double foo2(uint32_t a)
{
	return mull(double(a));
}

void mul(uint32_t* a, uint32_t* c)
{
	c[0] = a[0] * FNV_PRIME;
	double x = (double)a[1];
	double y = (double)FNV_PRIME;
	double ans = x * y;
	c[1] = uint32_t(ans);
}

int main()
{
	uint32_t x = 988451793;
	uint32_t a[2], c[2];
	a[0] = x, c[0] = 0;
	a[1] = x, c[1] = 0;
	mul(a, c);
	std::cout << c[0] << " " << c[1] << "\n";
	//test_all();
	//compute_hash_(31415, myans);
	//2420552195 4294967295
	// 9046B203 
	//std::cout <<  a << "\n";
	//uint64_t ans1 = foo0(a);
	//auto ans2 = foo2(a);
	//std::cout << (ans1 == ans2) << " " << ans1 << " " << ans2 << " " << int64_t(ans2 - ans1) << "\n";
	//uint32_t ans1_ = ans1;
	//uint32_t ans2_ = uint64_t(ans2) & 0xffffffff;
	//std::cout << (ans1_ == ans2_) << " " << ans1_ << " " << ans2_ << " " << int64_t(ans2_ - ans1_) << "\n";
	return 0;
}
