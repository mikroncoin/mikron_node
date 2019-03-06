#!/usr/bin/env bash

set -o errexit
set -o xtrace

bootstrapArgs=()
while getopts 'm' OPT; do
	case "${OPT}" in
		m)
			bootstrapArgs+=('--with-libraries=thread,log,filesystem,program_options')
			;;
	esac
done

BOOST_BASENAME=boost_1_67_0
BOOST_ROOT=${BOOST_ROOT-/usr/local/boost}
BOOST_URL=https://downloads.sourceforge.net/project/boost/boost/1.67.0/${BOOST_BASENAME}.tar.bz2
BOOST_ARCHIVE="${BOOST_BASENAME}.tar.bz2"
BOOST_ARCHIVE_SHA256='2684c972994ee57fc5632e03bf044746f6eb45d4920c343937a465fd67a5adba'

wget --quiet --no-check-certificate -O "${BOOST_ARCHIVE}.new" "${BOOST_URL}"
checkHash="$(openssl dgst -sha256 "${BOOST_ARCHIVE}.new" | sed 's@^.*= *@@')"
if [ "${checkHash}" != "${BOOST_ARCHIVE_SHA256}" ]; then
	echo "Checksum mismatch.  Expected ${BOOST_ARCHIVE_SHA256}, got ${checkHash}" >&2

	exit 1
fi
mv "${BOOST_ARCHIVE}.new" "${BOOST_ARCHIVE}"

tar xf "${BOOST_ARCHIVE}"
cd ${BOOST_BASENAME}
./bootstrap.sh "${bootstrapArgs[@]}"
./b2 -d2 -j $(nproc) --prefix=${BOOST_ROOT} \
	variant=release \
	link=static \
	threading=multi \
	address-model=64 \
	architecture=x86 \
	instruction-set=nocona \
	cxxflags="-mno-sse3 -mno-ssse3 -mno-avx -fvisibility=hidden" \
	install
cd ..
rm -rf ${BOOST_BASENAME}
rm -f "${BOOST_ARCHIVE}"
