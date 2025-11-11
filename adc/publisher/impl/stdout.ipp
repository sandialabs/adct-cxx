#include <adc/builder/builder.hpp>
#include <adc/publisher/publisher.hpp>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <filesystem>

namespace adc {

std::map< const std::string, const std::string> stdout_defaults;

/*! \brief Terminal output publisher_api implementation.
  This plugin sends messages to stdout synchronously.
  Multiple independent instances of this plugin may be used simultaneously,
  but message integrity depends on the behavior of stdout.
 */
class stdout_plugin : public publisher_api {
	enum state {
		ok,
		err
	};

private:
	const std::string vers;
	const std::vector<std::string> tags;
	enum state state;
	bool paused;

public:
	stdout_plugin() : vers("1.0.0") , tags({"none"}), state(ok), paused(false) { }

        int publish(std::shared_ptr<builder_api> b) {
		if (paused)
			return 0;
		if (state != ok)
			return 1;
		std::cout << b->serialize() << std::endl;
		return 0;
	}

        int config(const std::map< std::string, std::string >& /* m */) {
		return  0;
	}

        int config(const std::map< std::string, std::string >& /* m */, std::string_view /* env_prefix */) {
		return 0;
	}

	const std::map< const std::string, const std::string> & get_option_defaults() {
		return stdout_defaults;
	}

        int initialize() {
		return 0;
	}

        void finalize() {
	}

	void pause() {
		paused = true;
	}

        void resume() {
		paused = false;
	}

	std::string_view name() const {
		return "stdout";
	}

	std::string_view version() const {
		return vers;
	}

	~stdout_plugin() { }
};


} // adc
