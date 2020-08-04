TRACE_DIR=./trace/macrobench
RESULT_DIR=./result

workloads="fileserver linkbench tpc_c varmail webserver ycsb_a ycsb_b ycsb_c ycsb_d ycsb_e ycsb_f" 
workloads="tpc_c"

#for trc_file in $TRACE_DIR/*.trc; do

for wk in $workloads; do
	tfname=$TRACE_DIR/"$wk".trc
	rfname=$RESULT_DIR/"$wk".rslt
	ffname=$RESULT_DIR/"$wk".dirty

	
	echo "./driver $tfname > $rfname"
	./driver $tfname > $rfname

	grep "DIRTY_PAGES" $rfname | awk '{print $NF}' > $ffname

done



