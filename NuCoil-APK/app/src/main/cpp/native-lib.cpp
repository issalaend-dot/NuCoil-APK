#include <jni.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <string>
#include <map>

constexpr double PI = 3.14159265358979323846;

struct ParametrosProceso {
    int modoEntalpia = 1;            // 1: Estequiometrico (combustiones), 2: Directo
    double dhCombSustrato = 2805.0;  // kJ/mol sustrato (glucosa)
    double dhCombBiomasa = 560.0;    // kJ/C-mol biomasa
    double rendimientoYxs = 3.0;     // C-mol biomasa / mol sustrato
    double dhDirecto = 460.0;        // kJ/mol referencia
    double tasaMolar = 15.0;         // mol/h
    double factorCalor = 1.0;        // Fraccion metabolica liberada

    double diametroTanque = 1.20;    // m
    double volumenLiquido = 1.10;    // m^3
    double diametroImpulsor = 0.40;  // m
    double velocidadAgitacion = 2.5; // rps (150 rpm)
    double numeroPotencia = 5.0;     // Np (Rushton)
    double flujoAire = 0.010;        // m^3/s
    double rhoCaldo = 1020.0;        // kg/m^3
    double muCaldo = 0.0020;         // Pa*s
    double kCaldo = 0.60;            // W/(m*K)
    double cpCaldo = 4100.0;         // J/(kg*K)

    double tCaldo = 30.0;            // °C
    double tRefIn = 10.0;            // °C
    double tRefOut = 18.0;           // °C
    double flujoRefDisp = 0.25;      // kg/s
    double cpRef = 4185.0;           // J/(kg*K)
    double rhoRef = 999.0;           // kg/m^3
    double muRef = 0.00117;          // Pa*s
    double kRef = 0.589;             // W/(m*K)

    double dExtTubo = 0.025;         // m
    double espesorPared = 0.002;     // m
    double kPared = 16.0;            // W/(m*K)
    double factorEnsuciamiento = 0.0002; // m^2*K/W
    double holguraPared = 0.10;      // m
    double pasoVueltas = 0.075;      // m
};

struct ResultadosDiseno {
    bool valido = true;
    std::string mensajeError;

    double dhReaccion = 0.0;
    double qReaccion = 0.0;
    double reAgitacion = 0.0;
    double potenciaNoAir = 0.0;
    double potenciaAireada = 0.0;
    double qTotal = 0.0;

    double dtlm = 0.0;
    double ho = 0.0;
    double flujoRefReq = 0.0;
    double velRef = 0.0;
    double reRef = 0.0;
    double hi = 0.0;
    double uGlobal = 0.0;

    double areaReq = 0.0;
    double longitudTubo = 0.0;
    double diametroHelice = 0.0;
    double numVueltas = 0.0;
    double alturaSerpentin = 0.0;
    double alturaLiquido = 0.0;
    double alturaTanqueEst = 0.0;
    double caidaPresion = 0.0;
    bool cabeEnLiquido = true;
    bool libreImpulsor = true;
    bool caudalSuficiente = true;
};

ResultadosDiseno calcularDiseno(const ParametrosProceso& p) {
    ResultadosDiseno r;

    if (p.diametroTanque <= 0.05 || p.volumenLiquido <= 0.001 || p.diametroImpulsor <= 0.01 ||
        p.velocidadAgitacion <= 0.0 || p.dExtTubo <= 0.002 || p.pasoVueltas <= 0.002 ||
        p.rhoCaldo <= 0.0 || p.muCaldo <= 0.0 || p.kCaldo <= 0.0 || p.cpCaldo <= 0.0 ||
        p.rhoRef <= 0.0 || p.muRef <= 0.0 || p.kRef <= 0.0 || p.cpRef <= 0.0 || p.kPared <= 0.0) {
        r.valido = false;
        r.mensajeError = "Todas las dimensiones y propiedades fisicas deben ser mayores a cero.";
        return r;
    }

    if (p.tCaldo <= p.tRefIn || p.tCaldo <= p.tRefOut || p.tRefOut <= p.tRefIn) {
        r.valido = false;
        r.mensajeError = "Error termodinamico: Se requiere T_caldo > T_salida > T_entrada.";
        return r;
    }

    double dIntTubo = p.dExtTubo - 2.0 * p.espesorPared;
    if (dIntTubo <= 0.001) {
        r.valido = false;
        r.mensajeError = "Error geometrico: El espesor de pared supera el radio interior del tubo.";
        return r;
    }

    // 1. Balance de energia y entalpia de reaccion (Doran Cap. 5 y 8)
    if (p.modoEntalpia == 1) {
        r.dhReaccion = p.dhCombSustrato - (p.rendimientoYxs * p.dhCombBiomasa);
    } else {
        r.dhReaccion = p.dhDirecto;
    }
    if (r.dhReaccion < 0.0) r.dhReaccion = 0.0;

    r.qReaccion = (r.dhReaccion * 1000.0 * p.tasaMolar * p.factorCalor) / 3600.0;
    r.reAgitacion = (p.rhoCaldo * p.velocidadAgitacion * std::pow(p.diametroImpulsor, 2)) / p.muCaldo;
    r.potenciaNoAir = p.numeroPotencia * p.rhoCaldo * std::pow(p.velocidadAgitacion, 3) * std::pow(p.diametroImpulsor, 5);

    if (p.flujoAire > 0.0) {
        double term = (std::pow(r.potenciaNoAir, 2) * p.velocidadAgitacion * std::pow(p.diametroImpulsor, 3))
                      / std::pow(p.flujoAire, 0.56);
        r.potenciaAireada = 0.706 * std::pow(term, 0.45);
        if (r.potenciaAireada > r.potenciaNoAir) r.potenciaAireada = r.potenciaNoAir;
    } else {
        r.potenciaAireada = r.potenciaNoAir;
    }

    r.qTotal = r.qReaccion + r.potenciaAireada;
    if (r.qTotal <= 1.0) {
        r.valido = false;
        r.mensajeError = "La carga termica total debe ser positiva para dimensionar el enfriamiento.";
        return r;
    }

    // 2. Fuerza impulsora DTLM
    double dt1 = p.tCaldo - p.tRefIn;
    double dt2 = p.tCaldo - p.tRefOut;
    r.dtlm = (std::abs(dt1 - dt2) < 1e-6) ? 0.5 * (dt1 + dt2) : (dt1 - dt2) / std::log(dt1 / dt2);

    // 3. Coeficientes de transferencia (Doran Fig. 9.17 / Prob. 9.13 + Gnielinski)
    double prCaldo = (p.cpCaldo * p.muCaldo) / p.kCaldo;
    double nuCaldo = 0.87 * std::pow(r.reAgitacion, 0.62) * std::pow(prCaldo, 1.0 / 3.0);
    r.ho = (nuCaldo * p.kCaldo) / p.diametroTanque;

    r.flujoRefReq = r.qTotal / (p.cpRef * (p.tRefOut - p.tRefIn));
    r.caudalSuficiente = (p.flujoRefDisp >= r.flujoRefReq);

    double areaTransversal = PI * std::pow(dIntTubo, 2) / 4.0;
    r.velRef = r.flujoRefReq / (p.rhoRef * areaTransversal);
    r.reRef = (p.rhoRef * r.velRef * dIntTubo) / p.muRef;
    double prRef = (p.cpRef * p.muRef) / p.kRef;

    r.diametroHelice = p.diametroTanque - 2.0 * p.holguraPared - p.dExtTubo;
    if (r.diametroHelice <= dIntTubo) {
        r.valido = false;
        r.mensajeError = "Error geometrico: La holgura radial es demasiado grande para el tanque.";
        return r;
    }

    double fDarcy = 0.0;
    double nuInt = 0.0;
    if (r.reRef < 2300.0) {
        fDarcy = 64.0 / r.reRef;
        nuInt = 3.66;
    } else {
        fDarcy = std::pow(0.79 * std::log(r.reRef) - 1.64, -2.0);
        double num = (fDarcy / 8.0) * (r.reRef - 1000.0) * prRef;
        double den = 1.0 + 12.7 * std::sqrt(fDarcy / 8.0) * (std::pow(prRef, 2.0 / 3.0) - 1.0);
        double nuRecto = num / den;
        nuInt = nuRecto * (1.0 + 3.5 * (dIntTubo / r.diametroHelice));
    }
    r.hi = (nuInt * p.kRef) / dIntTubo;

    double resTotal = (1.0 / r.ho) + p.factorEnsuciamiento +
                      ((p.dExtTubo * std::log(p.dExtTubo / dIntTubo)) / (2.0 * p.kPared)) +
                      (p.dExtTubo / (dIntTubo * r.hi));
    r.uGlobal = 1.0 / resTotal;

    // 4. Geometria del serpentin e hidraulica
    r.areaReq = r.qTotal / (r.uGlobal * r.dtlm);
    r.longitudTubo = r.areaReq / (PI * p.dExtTubo);

    double longPorVuelta = std::sqrt(std::pow(PI * r.diametroHelice, 2) + std::pow(p.pasoVueltas, 2));
    r.numVueltas = r.longitudTubo / longPorVuelta;
    r.alturaSerpentin = r.numVueltas * p.pasoVueltas;
    r.alturaLiquido = (4.0 * p.volumenLiquido) / (PI * std::pow(p.diametroTanque, 2));
    r.alturaTanqueEst = std::max(r.alturaLiquido * 1.25, p.diametroTanque * 1.5);

    double factorIto = 1.0 + 0.0823 * std::pow(dIntTubo / r.diametroHelice, 0.53) * std::pow(r.reRef, 0.25);
    r.caidaPresion = ((fDarcy * factorIto) * (r.longitudTubo / dIntTubo) * (0.5 * p.rhoRef * std::pow(r.velRef, 2))) / 1000.0;

    r.cabeEnLiquido = (r.alturaSerpentin < r.alturaLiquido);
    r.libreImpulsor = (r.diametroHelice > p.diametroImpulsor * 1.15);

    return r;
}

std::string escaparJson(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

std::string resultadosAJson(const ParametrosProceso& p, const ResultadosDiseno& r) {
    std::ostringstream ss;
    ss.imbue(std::locale::classic());
    ss << std::fixed << std::setprecision(4);
    ss << "{"
       << "\"valido\":" << (r.valido ? "true" : "false") << ","
       << "\"mensajeError\":\"" << escaparJson(r.mensajeError) << "\","
       << "\"dhReaccion\":" << r.dhReaccion << ","
       << "\"qReaccionKw\":" << (r.qReaccion / 1000.0) << ","
       << "\"reAgitacion\":" << r.reAgitacion << ","
       << "\"potenciaNoAirKw\":" << (r.potenciaNoAir / 1000.0) << ","
       << "\"potenciaAireadaKw\":" << (r.potenciaAireada / 1000.0) << ","
       << "\"qTotalKw\":" << (r.qTotal / 1000.0) << ","
       << "\"dtlm\":" << r.dtlm << ","
       << "\"ho\":" << r.ho << ","
       << "\"flujoRefReq\":" << r.flujoRefReq << ","
       << "\"velRef\":" << r.velRef << ","
       << "\"reRef\":" << r.reRef << ","
       << "\"hi\":" << r.hi << ","
       << "\"uGlobal\":" << r.uGlobal << ","
       << "\"areaReq\":" << r.areaReq << ","
       << "\"longitudTubo\":" << r.longitudTubo << ","
       << "\"diametroHelice\":" << r.diametroHelice << ","
       << "\"numVueltas\":" << r.numVueltas << ","
       << "\"alturaSerpentin\":" << r.alturaSerpentin << ","
       << "\"alturaLiquido\":" << r.alturaLiquido << ","
       << "\"alturaTanqueEst\":" << r.alturaTanqueEst << ","
       << "\"diametroTanque\":" << p.diametroTanque << ","
       << "\"diametroImpulsor\":" << p.diametroImpulsor << ","
       << "\"dExtTubo\":" << p.dExtTubo << ","
       << "\"pasoVueltas\":" << p.pasoVueltas << ","
       << "\"caidaPresion\":" << r.caidaPresion << ","
       << "\"cabeEnLiquido\":" << (r.cabeEnLiquido ? "true" : "false") << ","
       << "\"libreImpulsor\":" << (r.libreImpulsor ? "true" : "false") << ","
       << "\"caudalSuficiente\":" << (r.caudalSuficiente ? "true" : "false")
       << "}";
    return ss.str();
}

ParametrosProceso parsearParametros(const std::string& target) {
    ParametrosProceso p;
    size_t qPos = target.find('?');
    if (qPos == std::string::npos) return p;

    std::string query = target.substr(qPos + 1);
    std::istringstream stream(query);
    std::string par;
    std::map<std::string, double> kv;

    while (std::getline(stream, par, '&')) {
        size_t eq = par.find('=');
        if (eq != std::string::npos) {
            std::string k = par.substr(0, eq);
            std::string v = par.substr(eq + 1);
            try { kv[k] = std::stod(v); } catch (...) {}
        }
    }

    auto get = [&](const std::string& key, double& dest) {
        if (kv.count(key)) dest = kv[key];
    };

    if (kv.count("modoEntalpia")) p.modoEntalpia = static_cast<int>(kv["modoEntalpia"]);
    get("dhCombSustrato", p.dhCombSustrato);
    get("dhCombBiomasa", p.dhCombBiomasa);
    get("rendimientoYxs", p.rendimientoYxs);
    get("dhDirecto", p.dhDirecto);
    get("tasaMolar", p.tasaMolar);
    get("factorCalor", p.factorCalor);

    get("diametroTanque", p.diametroTanque);
    get("volumenLiquido", p.volumenLiquido);
    get("diametroImpulsor", p.diametroImpulsor);
    get("velocidadAgitacion", p.velocidadAgitacion);
    get("numeroPotencia", p.numeroPotencia);
    get("flujoAire", p.flujoAire);
    get("rhoCaldo", p.rhoCaldo);
    get("muCaldo", p.muCaldo);
    get("kCaldo", p.kCaldo);
    get("cpCaldo", p.cpCaldo);

    get("tCaldo", p.tCaldo);
    get("tRefIn", p.tRefIn);
    get("tRefOut", p.tRefOut);
    get("flujoRefDisp", p.flujoRefDisp);
    get("cpRef", p.cpRef);
    get("rhoRef", p.rhoRef);
    get("muRef", p.muRef);
    get("kRef", p.kRef);

    get("dExtTubo", p.dExtTubo);
    get("espesorPared", p.espesorPared);
    get("kPared", p.kPared);
    get("factorEnsuciamiento", p.factorEnsuciamiento);
    get("holguraPared", p.holguraPared);
    get("pasoVueltas", p.pasoVueltas);

    return p;
}


extern "C" JNIEXPORT jstring JNICALL
Java_com_nucoil_app_MainActivity_nativeCalcular(
        JNIEnv* env,
        jobject /* this */,
        jstring queryJstr) {
    const char* raw = env->GetStringUTFChars(queryJstr, nullptr);
    std::string query = raw ? raw : "";
    env->ReleaseStringUTFChars(queryJstr, raw);

    ParametrosProceso p = parsearParametros("?" + query);
    ResultadosDiseno r = calcularDiseno(p);
    std::string json = resultadosAJson(p, r);
    return env->NewStringUTF(json.c_str());
}
