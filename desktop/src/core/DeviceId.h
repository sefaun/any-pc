#pragma once

#include <QString>

namespace anypc::DeviceId {

/** Removes every non digit character ("482 913 507" -> "482913507"). */
QString normalize(const QString &input);

/** True for a canonical 9 digit device id that does not start with 0. */
bool isValid(const QString &normalized);

/** Groups digits for display ("482913507" -> "482 913 507"). */
QString format(const QString &normalized);

} // namespace anypc::DeviceId
