#ifndef NON_COPIABLE_H_
#define NON_COPIABLE_H_

namespace astro {
    class NonCopiable {
    public:
        NonCopiable() {};

        ~NonCopiable() {};
    private:
        /*it's the same as =delete after declaration when they are public*/
        NonCopiable(const NonCopiable &) = delete;

        NonCopiable operator=(const NonCopiable &) = delete;
    };
}


#endif // !NON_COPIABLE_H_
