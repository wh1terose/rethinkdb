#ifndef CONTAINERS_ARCHIVE_VERSIONED_HPP_
#define CONTAINERS_ARCHIVE_VERSIONED_HPP_

#include "version.hpp"
#include "containers/archive/archive.hpp"

ARCHIVE_PRIM_MAKE_RANGED_SERIALIZABLE(cluster_version_t, uint8_t,
                                      cluster_version_t::v1_13,
                                      cluster_version_t::LATEST_VERSION);

// Serializes a value for a given version.  DOES NOT SERIALIZE THE VERSION NUMBER!
template <class T>
void serialize_for_version(cluster_version_t version, write_message_t *wm,
                           const T &value) {
    // Right now, since there's only one version number, we can just call the normal
    // serialization function.
    rassert(version == cluster_version_t::v1_13);
    serialize(wm, value);
}

// Deserializes a value, assuming it's serialized for a given version.  (This doesn't
// deserialize any version numbers.)
template <class T>
archive_result_t deserialize_for_version(cluster_version_t version,
                                         read_stream_t *s,
                                         T *thing) {
    // Right now, since there's only one version number, we can just call the normal
    // serialization function.
    rassert(version == cluster_version_t::v1_13);
    return deserialize(s, thing);
}



#endif  // CONTAINERS_ARCHIVE_VERSIONED_HPP_
