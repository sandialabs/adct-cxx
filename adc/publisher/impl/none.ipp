#include <adc/builder/builder.hpp>
#include <adc/publisher/publisher.hpp>

namespace adc {


/*! \brief Message suppression publisher; it quietly ignores all
  publication requests.
 */
class none_plugin : public publisher_api {

private:
	const std::string vers;
	const std::vector<std::string> tags;
	std::map< const std::string, const std::string> none_defaults;

public:
	none_plugin() : vers("1.0.0") , tags({"none"}) { }

        int publish(std::shared_ptr<builder_api> /* b */) {
		return 0;
	}

        int config(const std::map< std::string, std::string >& /* m */) {
		return  0;
	}

        int config(const std::map< std::string, std::string >& /* m */, std::string_view /* env_prefix */) {
		return 0;
	}

	const std::map< const std::string, const std::string> & get_option_defaults() {
		return none_defaults;
	}

        int initialize() {
		return 0;
	}

        void finalize() {
	}

	void pause() {
	}

        void resume() {
	}

	std::string_view name() const {
		return "none";
	}

	std::string_view version() const {
		return vers;
	}

	~none_plugin() { }
};


} // adc
