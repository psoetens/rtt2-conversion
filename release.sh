
version="$1"

if [ x$version = x ]; then
    echo "usage: $0 <version>"
    exit 1
fi

mkdir -p export

git archive --format=tar --prefix=rtt2-converter-$version/ HEAD | gzip >export/rtt2-converter-$version.tar.gz
git archive --format=zip --prefix=rtt2-converter-$version/ HEAD > export/rtt2-converter-$version.zip
git archive --format=tar --prefix=rtt2-converter-$version/ HEAD | bzip2 >export/rtt2-converter-$version.tar.bz2

git tag -f -a -m"Version $version" v$version
