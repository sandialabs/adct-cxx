#include <adc/adc.hpp>
#include <iostream>
int main() {
	adc::factory f;
	auto m = f.get_multi_publisher();
	auto x = f.get_publisher("file");
	auto y = f.get_publisher("flubber");
	auto z = f.get_publisher("stdout");
	if (x)
		std::cout << "x ok" << std::endl;
	else
		std::cout << "x null" << std::endl;
	if (y)
		std::cout << "y ok" << std::endl;
	else
		std::cout << "y null" << std::endl;
	if (z)
		std::cout << "z ok" << std::endl;
	else
		std::cout << "z null" << std::endl;
	m->add(x);
	m->add(y);
	m->add(z);
	auto names = m->get_names();
	for (auto e : names) {
		std::cout << "pub: " << e << std::endl;
	}
	return 0;
}
