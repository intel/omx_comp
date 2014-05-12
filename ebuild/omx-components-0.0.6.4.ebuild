# Based on the Gentoo ebuild:
# Copyright 1999-2012 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI="4"

RESTRICT="nomirror"
# libmix bin repo has to be created
LIBMIX_REPO='media-libs/intel-libmix'

inherit autotools cros-workon

DESCRIPTION="Khronos openMAX IL Components library"
HOMEPAGE="https://github.com/01org/omx_comp"

if [[ "${PV}" != "9999" ]] ; then
SRC_URI=https://github.com/01org/omx_comp/archive/${P}.tar.gz
else
CROS_WORKON_LOCALNAME="../partner_private/omx-components"
fi

LICENSE="Apache-2.0"
SLOT="0"
KEYWORDS="-* amd64 x86"
IUSE="omx-debug"

RDEPEND="${LIBMIX_REPO}
	 media-libs/omxil-core"

DEPEND="${LIBMIX_REPO}
	virtual/pkgconfig"

if [[ "${PV}" != "9999" ]] ; then
src_unpack() {
	default
	# tag and project names are fixed in github
	S=${WORKDIR}/omx_comp-${P}
}
fi

src_prepare() {
	eautoreconf
}

src_configure() {
	econf $(use_enable omx-debug)
}

src_install() {
	default
	prune_libtool_files
}
