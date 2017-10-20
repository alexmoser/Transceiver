# invoke SourceDir generated makefile for packetTx.pem3
packetTx.pem3: .libraries,packetTx.pem3
.libraries,packetTx.pem3: package/cfg/packetTx_pem3.xdl
	$(MAKE) -f /Users/alex/workspace_v7/packetTX/src/makefile.libs

clean::
	$(MAKE) -f /Users/alex/workspace_v7/packetTX/src/makefile.libs clean

