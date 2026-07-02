#ifndef AXDP_OBSERVER_H
#define AXDP_OBSERVER_H

#include <memory>
#include <map>
#include <functional>

namespace axdp {

    template<class T>
    class Observer {
    public:
        explicit Observer(bool removable = false) : removable_(removable) {

        }

        virtual ~Observer() {

        }

        virtual int update(T *t) {
            return 1;
        }

        bool isRemovable() { return removable_; }

    protected:
        bool removable_;
    };


    template<class T>
    class Observable {
    public:
        Observable() {}

        virtual ~Observable() {
            for (auto it = observer_list_.begin(); it != observer_list_.end(); ++it) {
                if (it->first->isRemovable()) {
                    delete it->first;
                };
            }
            observer_list_.clear();
        }

        void add(Observer<T> *observer) {
            observer_list_[observer] = std::bind(&Observer<T>::update, observer, std::placeholders::_1);
        }

        void remove(Observer<T> *observer) {
            observer_list_.erase(observer);
        }

        void notify(T *t) {
            for (auto it = observer_list_.begin(); it != observer_list_.end(); ++it) {
                it->second(t);
            }
        }

        bool contains(Observer<T> *observer) {
            return observer_list_.find(observer) != observer_list_.end();
        }

    private:
        std::map<Observer<T> *, std::function<int(T *)>> observer_list_;
    };


}


#endif //AXDP_OBSERVER_H
