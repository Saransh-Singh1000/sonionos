#include <stdio.h>
#include "usyscall.h"

class App {
public:
    const char *title;
    App(const char *t) : title(t) {}
    void run() {
        printf("[C++ App] %s running at CPL = %u!\n", title, get_current_cpl());
    }
};

int main(void) {
    App *a = new App("sonionOS C++17 Engine");
    a->run();
    delete a;
    return 0;
}
