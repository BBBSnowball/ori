cd $TEMP_DIR
$ORI_EXE replicate --shallow $SOURCE_FS $TEST_FS

orifs $SOURCE_FS
orifs $TEST_FS

cd $TEST_FS
$PYTHON $SCRIPTS/compare.py "$SOURCE_FS" "$TEST_FS"
cd ..

$UMOUNT $SOURCE_FS
$UMOUNT $TEST_FS

cd ~/.ori/$TEST_FS.ori
$ORIDBG_EXE verify
$ORIDBG_EXE stats

#bash -e $SCRIPTS/verify_refcounts.sh

cd $TEMP_DIR
$ORI_EXE removefs $TEST_FS

