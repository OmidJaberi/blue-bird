#ifndef BB_PERSIST_MODEL_POSTGRES_H
#define BB_PERSIST_MODEL_POSTGRES_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/persist/model.h"

/* PostgreSQL model backend (name: "postgres").
 *
 * Only available when Blue-Bird is configured with -DBB_WITH_POSTGRES=ON
 * (requires libpq). open() takes a libpq connection string, either a URI
 * ("postgresql://user:pass@host:5432/dbname") or key/value form
 * ("host=localhost dbname=app user=app"). */
const bb_model_api_t *bb_model_postgres_api(void);


#ifdef __cplusplus
}
#endif

#endif //BB_PERSIST_MODEL_POSTGRES_H
