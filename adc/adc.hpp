#ifndef adc_hpp
#define adc_hpp
#include "adc/adc_config.h"
#include "adc/factory.hpp"

/** \brief Application Data Collection: Structured logging for scientific computing.
\mainpage Application Data Collection (C++)

\section purpose Purpose (why)
The ADC project aims to provide workflow metadata collection for all scientific computing environments and workflow stages.
Here 'metadata' means small, structured descriptions of:
 - what is planned (such as a campaign of multiple jobs, or in a single job being submitted)
 - what is happening (such as the data sizes and features in an individual execution or rank of a parallel execution)
 - what has happened (such as scalar progress indicators or final results)

These *complement* large data flows such as:
 - time-series hardware metrics (papi, LDMS)
 - call-profiling data (caliper, kokkos, gprof, ...)
 - lists of particle or geometric entities and associated engineering or physics data (hdf5, netcdf, exodus, commercial CAD formats)
 .
The metadata logged is intended for analysis during or after the current workflow step to allow:
  - other tooling to influence subsequent steps, such as compute resource selection.
  - users to track workflow progress.
  - performance analysts to correlate and predict performance based on simulation metadata.
  - software developers and managers to prioritize feature work based on usage.
  - software agents to detect when system performance is below expectations.

\section data Output (what)
ADC produces *semi-structured*, extensible json messages. Some json objects have standardized names and contents; these
are included in a message if a convenience function is called to add them to the message. If specific
names are present, the associated values have standard origins.
Other objects have minimally defined fields, with the rest of the fields being up to the application.
Where common json libraries exist, the application may also attach arbitrary json objects to a message; this support
varies by implementation language. The \ref adc::builder_api provides a rich set of convenience functions for constructing
and customizing ADC messages that capture scientific programming data types.

\section environments Environments (where)
Scientific workflows span desktop, server, and cluster environments. Critical information
is usually added (or first exposed) at each stage. Linux environments are directly supported
as well as Windows environments connected through Java.

\section bindings Languages (how)
Initially, C++, Python, Java are supported, and likely this will expand to C, Fortran and JavaScript.
Documented here is the C++17 standard library-based binding that has optional support for MPI.
This binding follows the Factory pattern (\ref adc::factory). The factory provides json builder objects and publisher objects.

A header-only binding (which, however, adds a boost dependency to the application build) is also in development.

\section library Linking
- Linking with -ladc-c++ provides a factory-based API without MPI dependencies.
- Linking with -ladc-c++-mpi provides a factory-based API with MPI library dependencies.

\section publishers Performance
The data collected is published as the application runs, and is fully labeled with
data types for downstream interpretation up to decades into the future.

A variety of plugins implement the \ref adc::publisher_api 
to ensure transmission performance and overhead demands are met
for each scientific environment. Messages can be sent directly to the REST-based infrastructure, or
(in HPC scenarios) routed through any bus which can support json (such as LDMS or a cache file).

\section apilist Just show me the code:
APIs:
- \ref adc::factory
- \ref adc::builder_api
- \ref adc::publisher_api

Examples:
- \ref mpiSimpleDemo.cpp demonstrates instrumenting a trivial MPI program.

 */
/**
\defgroup API Factory and interfaces for application use
*/
namespace adc {

} // namespace adc
#endif // adc_hpp
