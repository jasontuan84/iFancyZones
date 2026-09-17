#include "app/Application.h"

int main(int argc, char **argv)
{
    ifz::Application app(argc, argv);
    app.bootstrap();
    return app.exec();
}
