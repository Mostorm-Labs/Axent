#ifndef ASTRO_CALLBACK_H_
#define ASTRO_CALLBACK_H_

namespace astro {
	template<class CB_TYPE>
	class Callback
	{
	public:
		Callback() :cb_(nullptr), pdata_(nullptr) {

		}
		Callback(CB_TYPE cb, void* pdata) :cb_(cb), pdata_(pdata){

		}
		~Callback() {

		}

		void setData(CB_TYPE cb, void* pdata) {
			cb_ = cb;
			pdata_ = pdata;
		}

		template<class... Args>
		void exec(Args... args) {
			if (cb_) cb_(pdata_, args...);
		}

		template<class... Args>
		void operator() (Args... args) {
			if (cb_) cb_(pdata_, args...);
		}

	private:
		CB_TYPE cb_;
		void* pdata_;
	};

}


#endif // !AUX_CALLBACK_H_
