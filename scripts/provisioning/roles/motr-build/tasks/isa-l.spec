%undefine _disable_source_fetch

Name:		isa-l
Version:	%{_isa_l_version}
Release:	1%{?dist}
Summary:	Intel(R) Intelligent Storage Acceleration Library.

Group:		Applications
License:	BSD-3-Clause
URL:		https://github.com/intel/isa-l/
Source0:	https://github.com/intel/isa-l/archive/v%{_isa_l_version}.tar.gz

BuildRequires:	autoconf libtool yasm make
#Requires:

%description
ISA-L is a collection of optimized low-level functions targeting storage applications. ISA-L includes:
> Erasure codes - Fast block Reed-Solomon type erasure codes for any encode/decode matrix in GF(2^8).
> CRC - Fast implementations of cyclic redundancy check. Six different polynomials supported.
  iscsi32, ieee32, t10dif, ecma64, iso64, jones64.
> Raid - calculate and operate on XOR and P+Q parity found in common RAID implementations.
> Compression - Fast deflate-compatible data compression.
> De-compression - Fast inflate-compatible data compression.

%global debug_package %{nil}

%prep
%setup -q
./autogen.sh

%build
%configure
#./configure
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}

%files
%defattr(-,root,root)
/usr/include/isa-l.h
/usr/include/isa-l/
/usr/lib64/libisal.a
/usr/lib64/libisal.la
/usr/lib64/libisal.so*
/usr/lib64/pkgconfig/libisal.pc
/usr/bin/igzip
/usr/share/man/man1/igzip.1.gz

%changelog
