#include "mr_exppois.h"
#include "mr_strutils.h"


template <class T>
void mrutils::ExpPoisson<T>::print(_UNUSED const T milliTime, _UNUSED std::ostream& os) {
}

template <class T>
void mrutils::ExpPoisson<T>::plot(const T milliTime) {
    if (g == NULL) {
        g = new mrutils::Gnuplot();
        g->setGrid();

        mrutils::stringstream ss;
        ss << "ExpPoisson lambda estimate " << name;

        g->setLabels(ss.str().c_str(),"{/Symbol l}","Prob");
    }

    g->clear();

    const double b = (pow(kappa,milliTime - startTime) - 1)/lnkappa;
    const double a = alpha * pow(kappa,milliTime - lastAdd);
    const double mu = a/b;

    g->lines().plot(&mrutils::stats::dgamma,a,b,0,2*mu);

    g->draw();
}

template _API_ void mrutils::ExpPoisson<unsigned>::plot(const unsigned milliTime);
template _API_ void mrutils::ExpPoisson<double>::plot(const double milliTime);

template _API_ void mrutils::ExpPoisson<unsigned>::print(const unsigned milliTime, std::ostream& os);
template _API_ void mrutils::ExpPoisson<double>::print(const double milliTime, std::ostream& os);
