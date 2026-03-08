/* Copyright 2025 NTESS. See the top-level LICENSE.txt file for details.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef adc_publisher_ipp
#define adc_publisher_ipp

namespace adc {


class multi_publisher : public multi_publisher_api
{
public:
	enum state {
		ok,
		err
	};
private:
	const string vers;
        const std::vector<string> tags;
	enum state state;
	int debug;
	publisher_vector pvec;

public:
	multi_publisher() : vers("1.0.0") , tags({"none"}), state(ok), debug(0) {
		const char *env = getenv("ADC_MULTI_PUBLISHER_DEBUG");
		if (env && !strcmp(env,"1") ) {
			debug = 1;
		} else {
			debug = 0;
		}
	}


        string_view version() const {
                return vers;
        }

	void add(std::shared_ptr<publisher_api> p) {
		if (!p && debug) {
			std::cout <<
				"multi_publisher: null publisher add ignored."
				<< std::endl;
		}
		pvec.push_back(p);
		if (debug) {
			std::cout << "publisher added: "
			       	<< p->name() << std::endl;
		}
	}

	int publish(std::shared_ptr<builder_api> b)
       	{
		if (state != ok)
			return EBADFD;
		int err = 0;
		for (auto& element : pvec) {
			int e = element->publish(b);
			if (e) {
				err += 1;
				if (debug) {
					std::cout << "publish failed (" << e << 
						") for plugin "
				       		<< element->name() << std::endl;
				}
			}	
		}
		return err;
	}

	void terminate()
	{
		for (auto& element : pvec) {
			element->finalize();
		}
		pvec.clear();
	}

	void pause()
	{
		for (auto& element : pvec) {
			element->pause();
		}
	}

	void resume()
	{
		for (auto& element : pvec) {
			element->resume();
		}
	}
};  // class multi_publisher

} // namespace adc
#endif // adc_multi_publisher_ipp
