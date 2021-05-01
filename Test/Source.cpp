#include <iostream>
#include<string>

struct Point
{
	float x, y;
	Point operator-(const Point r) const {
		return {x-r.x, y-r.y};
	}
	Point operator+(const Point r) const
	{
		return { x + r.x, y + r.y };
	}
	float operator*(const Point r) const
	{
		return x * r.y - y * r.x;
	}
	std::string to_string()
	{
		return "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
	}
};
struct Rect
{
	Point M, B;  // halfed bounds
	float alpha;
	std::string to_string() {
		return "(" + M.to_string() + ", " + B.to_string() + ", " + std::to_string(alpha) + ")";
	}
};

void rotate(Point& Y, const Point X, float alpha)
{
	Y = Y - X;
	float oldx = Y.x, cosa = cos(alpha), sina = sin(alpha);
	Y.x = Y.x * cosa - Y.y * sina;
	Y.y = oldx * sina + Y.y * cosa;
	Y = Y + X;
}

void rotate(Rect& r, float alpha, const Point O)
{
	r.alpha += alpha;
	rotate(r.M, O, alpha);
}

bool check(const Point M, const Point A, const Point B, const Point X) {
	float x = (B - A) * (M - A), y = (B - A) * (X - A);
	return x * y < 0;
}

bool BCRectHalfIntersects(Rect p, Rect a)
{
	rotate(a, -p.alpha, p.M);
	rotate(p, -p.alpha, p.M);
	Point A = a.M - a.B, B = a.M + a.B, C = a.M + Point{ -a.B.x, a.B.y }, D = a.M + Point{ a.B.x, -a.B.y };
	Point pA = p.M - p.B, pB = p.M + p.B, pC = p.M + Point{ -p.B.x, p.B.y }, pD = p.M + Point{ p.B.x, -p.B.y };
	
	rotate(A, a.M, a.alpha);
	rotate(B, a.M, a.alpha);
	rotate(C, a.M, a.alpha);
	rotate(D, a.M, a.alpha);
	
	if (check(p.M, pA, pC, A) && check(p.M, pA, pC, B) && check(p.M, pA, pC, C) && check(p.M, pA, pC, D))
		return false;
	if (check(p.M, pA, pD, A) && check(p.M, pA, pD, B) && check(p.M, pA, pD, C) && check(p.M, pA, pD, D))
		return false;
	if (check(p.M, pB, pD, A) && check(p.M, pB, pD, B) && check(p.M, pB, pD, C) && check(p.M, pB, pD, D))
		return false;
	if (check(p.M, pB, pC, A) && check(p.M, pB, pC, B) && check(p.M, pB, pC, C) && check(p.M, pB, pC, D))
		return false;
	return true;
}
bool BCRectIntersects(const Rect& p, const Rect& a)
{
	return BCRectHalfIntersects(p, a) && BCRectHalfIntersects(a, p);
}

void test(const Rect a, const Rect b, bool ans)
{
	if (BCRectIntersects(a, b) != ans)
		std::cout << "Lox\n";
	else
		std::cout << "OK\n";
}

float to_deg(float grad) {
	return grad / 180.0 * 3.1415926f;
}

int main()
{
	test({ { 0, 0 }, { 1, 1 }, to_deg(0.0f) }, { { 2.0001, 0 }, { 1, 1 }, to_deg(0.0f) }, false);
	test({ { 0, 0 }, { 1, 1 }, to_deg(0.0f) }, { { 2.0001, 0 }, { 1, 1 }, to_deg(45.0f) }, true);

	test({ { 4.0f, 1.0f }, { 2.0f, 1.0f }, to_deg(0.0f) },
		{ { 5.0f, -2.0f }, { 2.0f, 1.0f }, to_deg(30.0f) }, false);
	test({ { 4.0f, 1.0f }, { 2.0f, 1.0f }, to_deg(0.0f) },
		{ { 5.0f, -2.0f }, { 2.0f, 1.0f }, to_deg(40.0f) }, true);
	test({ { 4.0f, 1.0f }, { 2.0f, 1.0f }, to_deg(0.0f) },
		{ { 5.0f, -2.0f }, { 2.0f, 1.0f }, to_deg(86.0f) }, true);
	test({ { 4.0f, 1.0f }, { 2.0f, 1.0f }, to_deg(0.0f) },
		{ { 5.0f, -2.0f }, { 2.0f, 1.0f }, to_deg(100.0f) }, true);
	test({ { 4.0f, 1.0f }, { 2.0f, 1.0f }, to_deg(0.0f) },
		{ { 5.0f, -2.0f }, { 2.0f, 1.0f }, to_deg(150.0f) }, false);

	test({ { 1.0f, 1.0f }, { 5.0f, 1.0f }, to_deg(332.0f) },
		{ { 7.0f, -2.0f }, { 2.0f, 1.0f }, to_deg(0.0f) }, true);
	test({ { 1.0f, 1.0f }, { 5.0f, 1.0f }, to_deg(332.0f) },
		{ { 7.0f, -2.0f }, { 2.0f, 1.0f }, to_deg(26.0f) }, true);
	test({ { 1.0f, 1.0f }, { 5.0f, 1.0f }, to_deg(332.0f) },
		{ { 7.0f, -2.0f }, { 2.0f, 1.0f }, to_deg(0.0f) }, true);
	test({ { 1.0f, 1.0f }, { 5.0f, 1.0f }, to_deg(332.0f) },
		{ { 7.0f, -2.0f }, { 2.0f, 1.0f }, to_deg(33.0f) }, true);
	test({ { 1.0f, 1.0f }, { 5.0f, 1.0f }, to_deg(332.0f) },
		{ { 7.0f, -2.0f }, { 2.0f, 1.0f }, to_deg(38.0f) }, false);
	test({ { 1.0f, 1.0f }, { 5.0f, 1.0f }, to_deg(332.0f) },
		{ { 7.0f, -2.0f }, { 2.0f, 1.0f }, to_deg(90.0f) }, false);
	test({ { 1.0f, 1.0f }, { 5.0f, 1.0f }, to_deg(332.0f) },
		{ { 7.0f, -2.0f }, { 2.0f, 1.0f }, to_deg(96.0f) }, true);
	test({ { 1.0f, 1.0f }, { 5.0f, 1.0f }, to_deg(332.0f) },
		{ { 7.0f, -2.0f }, { 2.0f, 1.0f }, to_deg(212.0f) }, true);

	test({ { 1.0f, 1.0f }, { 5.0f, 2.0f }, to_deg(100.0f) },
		{ { 6.0f, -4.0f }, { 2.0f, 1.0f }, to_deg(186.0f) }, false);
	test({ { 1.0f, 1.0f }, { 5.0f, 2.0f }, to_deg(152.0f) },
		{ { 6.0f, -4.0f }, { 2.0f, 1.0f }, to_deg(186.0f) }, false);
	test({ { 1.0f, 1.0f }, { 5.0f, 2.0f }, to_deg(160.0f) },
		{ { 6.0f, -4.0f }, { 2.0f, 1.0f }, to_deg(186.0f) }, false);
	test({ { 1.0f, 1.0f }, { 5.0f, 2.0f }, to_deg(102.0f) },
		{ { 6.0f, -4.0f }, { 2.0f, 1.0f }, to_deg(186.0f) }, true);
	test({ { 1.0f, 1.0f }, { 5.0f, 2.0f }, to_deg(106.0f) },
		{ { 6.0f, -4.0f }, { 2.0f, 1.0f }, to_deg(186.0f) }, true);
	test({ { 1.0f, 1.0f }, { 5.0f, 2.0f }, to_deg(110.0f) },
		{ { 6.0f, -4.0f }, { 2.0f, 1.0f }, to_deg(186.0f) }, true);
	test({ { 1.0f, 1.0f }, { 5.0f, 2.0f }, to_deg(140.0f) },
		{ { 6.0f, -4.0f }, { 2.0f, 1.0f }, to_deg(186.0f) }, true);
	test({ { 1.0f, 1.0f }, { 5.0f, 2.0f }, to_deg(146.0f) },
		{ { 6.0f, -4.0f }, { 2.0f, 1.0f }, to_deg(186.0f) }, true);
	test({ { 1.0f, 1.0f }, { 5.0f, 2.0f }, to_deg(150.0f) },
		{ { 6.0f, -4.0f }, { 2.0f, 1.0f }, to_deg(186.0f) }, true);
    return 0;
}
