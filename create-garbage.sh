mkdir -p sample
cd sample
src="/srv/allfactnobreak/reports"
dst="/srv/allfactnobreak/backup"

mkdir -p $src
mkdir -p $dst


mkdir -p $src/garbage
mkdir -p $src/garbage/garbage2


touch $src/examplefile.xml
touch $src/examplefile1.xml
touch $src/examplefile2.xml
touch $src/examplefile3.xml
touch $src/examplefile4.xml
touch $src/garbage/examplefile5.xml
touch $src/garbage/examplefile6.xml
touch $src/garbage/garbage2/examplefile7.xml


