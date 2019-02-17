// Data downloaded from http://www.m-bus.de/man.html
// 2019-02-17

#ifndef MANUFACTURERS_H
#define MANUFACTURERS_H
#define MANFCODE(a,b,c) ((a-64)*1024+(b-64)*32+(c-64))
#define LIST_OF_MANUFACTURERS \
X(ABB,MANFCODE('A','B','B'),ABB AB P.O. Box 1005 SE-61129 Nyköping Nyköping Sweden )\
X(ACE,MANFCODE('A','C','E'),Actaris (Elektrizität))\
X(ACG,MANFCODE('A','C','G'),Actaris (Gas))\
X(ACW,MANFCODE('A','C','W'),Actaris (Wasser und Wärme))\
X(AEG,MANFCODE('A','E','G'),AEG)\
X(AEL,MANFCODE('A','E','L'),Kohler Türkei)\
X(AEM,MANFCODE('A','E','M'),S.C. AEM S.A. Romania)\
X(AMP,MANFCODE('A','M','P'),Ampy Automation Digilog Ltd)\
X(AMT,MANFCODE('A','M','T'),Aquametro)\
X(APS,MANFCODE('A','P','S'),Apsis Kontrol Sistemleri Türkei)\
X(BEC,MANFCODE('B','E','C'),Berg Energiekontrollsysteme GmbH)\
X(BER,MANFCODE('B','E','R'),Bernina Electronic AG)\
X(BSE,MANFCODE('B','S','E'),Basari Elektronik A.S. Türkei)\
X(BST,MANFCODE('B','S','T'),BESTAS Elektronik Optik Türkei)\
X(CBI,MANFCODE('C','B','I'),Circuit Breaker Industries Südafrika)\
X(CLO,MANFCODE('C','L','O'),Clorius Raab Karcher Energie Service A/S)\
X(CON,MANFCODE('C','O','N'),Conlog)\
X(CZM,MANFCODE('C','Z','M'),Cazzaniga S.p.A.)\
X(DAN,MANFCODE('D','A','N'),Danubia)\
X(DFS,MANFCODE('D','F','S'),Danfoss A/S)\
X(DME,MANFCODE('D','M','E'),DIEHL Metering Industriestrasse 13 91522 Ansbach Germany )\
X(DZG,MANFCODE('D','Z','G'),Deutsche Zählergesellschaft)\
X(EDM,MANFCODE('E','D','M'),EDMI Pty.Ltd.)\
X(EFE,MANFCODE('E','F','E'),Engelmann Sensor GmbH)\
X(EKT,MANFCODE('E','K','T'),PA KVANT J.S. Russland)\
X(ELM,MANFCODE('E','L','M'),Elektromed Elektronik Ltd Türkei)\
X(ELS,MANFCODE('E','L','S'),ELSTER Produktion GmbH)\
X(EMH,MANFCODE('E','M','H'),EMH Elektrizitätszähler GmbH & CO KG)\
X(EMU,MANFCODE('E','M','U'),EMU Elektronik AG)\
X(EMO,MANFCODE('E','M','O'),Enermet)\
X(END,MANFCODE('E','N','D'),ENDYS GmbH)\
X(ENP,MANFCODE('E','N','P'),Kiev Polytechnical Scientific Research)\
X(ENT,MANFCODE('E','N','T'),ENTES Elektronik Türkei)\
X(ERL,MANFCODE('E','R','L'),Erelsan Elektrik ve Elektronik Türkei)\
X(ESM,MANFCODE('E','S','M'),Starion Elektrik ve Elektronik Türkei)\
X(EUR,MANFCODE('E','U','R'),Eurometers Ltd)\
X(EWT,MANFCODE('E','W','T'),Elin Wasserwerkstechnik)\
X(FED,MANFCODE('F','E','D'),Federal Elektrik Türkei)\
X(FML,MANFCODE('F','M','L'),Siemens Measurements Ltd.( Formerly FML Ltd.))\
X(GBJ,MANFCODE('G','B','J'),Grundfoss A/S)\
X(GEC,MANFCODE('G','E','C'),GEC Meters Ltd.)\
X(GSP,MANFCODE('G','S','P'),Ingenieurbuero Gasperowicz)\
X(GWF,MANFCODE('G','W','F'),Gas- u. Wassermessfabrik Luzern)\
X(HEG,MANFCODE('H','E','G'),Hamburger Elektronik Gesellschaft)\
X(HEL,MANFCODE('H','E','L'),Heliowatt)\
X(HTC,MANFCODE('H','T','C'),Horstmann Timers and Controls Ltd.)\
X(HYD,MANFCODE('H','Y','D'),Hydrometer GmbH)\
X(ICM,MANFCODE('I','C','M'),Intracom Griechenland)\
X(IDE,MANFCODE('I','D','E'),IMIT S.p.A.)\
X(INV,MANFCODE('I','N','V'),Invensys Metering Systems AG)\
X(ISK,MANFCODE('I','S','K'),Iskraemeco Slovenia)\
X(IST,MANFCODE('I','S','T'),ista Deutschland (bis 2005 Viterra Energy Services))\
X(ITR,MANFCODE('I','T','R'),Itron)\
X(IWK,MANFCODE('I','W','K'),IWK Regler und Kompensatoren GmbH)\
X(KAM,MANFCODE('K','A','M'),Kamstrup Energie A/S)\
X(KHL,MANFCODE('K','H','L'),Kohler Türkei)\
X(KKE,MANFCODE('K','K','E'),KK-Electronic A/S)\
X(KNX,MANFCODE('K','N','X'),KONNEX-based users (Siemens Regensburg))\
X(KRO,MANFCODE('K','R','O'),Kromschröder)\
X(KST,MANFCODE('K','S','T'),Kundo SystemTechnik GmbH)\
X(LEM,MANFCODE('L','E','M'),LEM HEME Ltd. UK)\
X(LGB,MANFCODE('L','G','B'),Landis & Gyr Energy Management (UK) Ltd.)\
X(LGD,MANFCODE('L','G','D'),Landis & Gyr Deutschland)\
X(LGZ,MANFCODE('L','G','Z'),Landis & Gyr Zug)\
X(LHA,MANFCODE('L','H','A'),Atlantic Meters Südafrika)\
X(LML,MANFCODE('L','M','L'),LUMEL Polen)\
X(LSE,MANFCODE('L','S','E'),Landis & Staefa electronic)\
X(LSP,MANFCODE('L','S','P'),Landis & Staefa production)\
X(LUG,MANFCODE('L','U','G'),Landis & Staefa)\
X(LSZ,MANFCODE('L','S','Z'),Siemens Building Technologies)\
X(MAD,MANFCODE('M','A','D'),Maddalena S.r.I. Italien)\
X(MEI,MANFCODE('M','E','I'),H. Meinecke AG (jetzt Invensys Metering Systems AG))\
X(MKS,MANFCODE('M','K','S'),MAK-SAY Elektrik Elektronik Türkei)\
X(MNS,MANFCODE('M','N','S'),MANAS Elektronik Türkei)\
X(MPS,MANFCODE('M','P','S'),Multiprocessor Systems Ltd Bulgarien)\
X(MTC,MANFCODE('M','T','C'),Metering Technology Corporation USA)\
X(NIS,MANFCODE('N','I','S'),Nisko Industries Israel)\
X(NMS,MANFCODE('N','M','S'),Nisko Advanced Metering Solutions Israel)\
X(NRM,MANFCODE('N','R','M'),Norm Elektronik Türkei)\
X(ONR,MANFCODE('O','N','R'),ONUR Elektroteknik Türkei)\
X(PAD,MANFCODE('P','A','D'),PadMess GmbH)\
X(PMG,MANFCODE('P','M','G'),Spanner-Pollux GmbH (jetzt Invensys Metering Systems AG))\
X(PRI,MANFCODE('P','R','I'),Polymeters Response International Ltd.)\
X(RAS,MANFCODE('R','A','S'),Hydrometer GmbH)\
X(REL,MANFCODE('R','E','L'),Relay GmbH)\
X(RKE,MANFCODE('R','K','E'),Raab Karcher ES (jetzt ista Deutschland))\
X(SAP,MANFCODE('S','A','P'),Sappel)\
X(SCH,MANFCODE('S','C','H'),Schnitzel GmbH)\
X(SEN,MANFCODE('S','E','N'),Sensus GmbH)\
X(SMC,MANFCODE('S','M','C'),&nbsp;)\
X(SME,MANFCODE('S','M','E'),Siame Tunesien)\
X(SML,MANFCODE('S','M','L'),Siemens Measurements Ltd.)\
X(SIE,MANFCODE('S','I','E'),Siemens AG)\
X(SLB,MANFCODE('S','L','B'),Schlumberger Industries Ltd.)\
X(SON,MANFCODE('S','O','N'),Sontex SA)\
X(SOF,MANFCODE('S','O','F'),softflow.de GmbH)\
X(SPL,MANFCODE('S','P','L'),Sappel)\
X(SPX,MANFCODE('S','P','X'),Spanner Pollux GmbH (jetzt Invensys Metering Systems AG))\
X(SVM,MANFCODE('S','V','M'),AB Svensk Värmemätning SVM)\
X(TCH,MANFCODE('T','C','H'),Techem Service AG)\
X(TIP,MANFCODE('T','I','P'),TIP Thüringer Industrie Produkte GmbH)\
X(UAG,MANFCODE('U','A','G'),Uher)\
X(UGI,MANFCODE('U','G','I'),United Gas Industries)\
X(VES,MANFCODE('V','E','S'),ista Deutschland (bis 2005 Viterra Energy Services))\
X(VPI,MANFCODE('V','P','I'),Van Putten Instruments B.V.)\
X(WMO,MANFCODE('W','M','O'),Westermo Teleindustri AB Schweden)\
X(YTE,MANFCODE('Y','T','E'),Yuksek Teknoloji Türkei)\
X(ZAG,MANFCODE('Z','A','G'),Zellwerg Uster AG)\
X(ZAP,MANFCODE('Z','A','P'),Zaptronix)\
X(ZIV,MANFCODE('Z','I','V'),ZIV Aplicaciones y Tecnologia S.A.)\
X(QDS,MANFCODE('Q','D','S'),QUNDIS GmbH)\

#define MANUFACTURER_ABB MANFCODE('A','B','B')
#define MANUFACTURER_ACE MANFCODE('A','C','E')
#define MANUFACTURER_ACG MANFCODE('A','C','G')
#define MANUFACTURER_ACW MANFCODE('A','C','W')
#define MANUFACTURER_AEG MANFCODE('A','E','G')
#define MANUFACTURER_AEL MANFCODE('A','E','L')
#define MANUFACTURER_AEM MANFCODE('A','E','M')
#define MANUFACTURER_AMP MANFCODE('A','M','P')
#define MANUFACTURER_AMT MANFCODE('A','M','T')
#define MANUFACTURER_APS MANFCODE('A','P','S')
#define MANUFACTURER_BEC MANFCODE('B','E','C')
#define MANUFACTURER_BER MANFCODE('B','E','R')
#define MANUFACTURER_BSE MANFCODE('B','S','E')
#define MANUFACTURER_BST MANFCODE('B','S','T')
#define MANUFACTURER_CBI MANFCODE('C','B','I')
#define MANUFACTURER_CLO MANFCODE('C','L','O')
#define MANUFACTURER_CON MANFCODE('C','O','N')
#define MANUFACTURER_CZM MANFCODE('C','Z','M')
#define MANUFACTURER_DAN MANFCODE('D','A','N')
#define MANUFACTURER_DFS MANFCODE('D','F','S')
#define MANUFACTURER_DME MANFCODE('D','M','E')
#define MANUFACTURER_DZG MANFCODE('D','Z','G')
#define MANUFACTURER_EDM MANFCODE('E','D','M')
#define MANUFACTURER_EFE MANFCODE('E','F','E')
#define MANUFACTURER_EKT MANFCODE('E','K','T')
#define MANUFACTURER_ELM MANFCODE('E','L','M')
#define MANUFACTURER_ELS MANFCODE('E','L','S')
#define MANUFACTURER_EMH MANFCODE('E','M','H')
#define MANUFACTURER_EMU MANFCODE('E','M','U')
#define MANUFACTURER_EMO MANFCODE('E','M','O')
#define MANUFACTURER_END MANFCODE('E','N','D')
#define MANUFACTURER_ENP MANFCODE('E','N','P')
#define MANUFACTURER_ENT MANFCODE('E','N','T')
#define MANUFACTURER_ERL MANFCODE('E','R','L')
#define MANUFACTURER_ESM MANFCODE('E','S','M')
#define MANUFACTURER_EUR MANFCODE('E','U','R')
#define MANUFACTURER_EWT MANFCODE('E','W','T')
#define MANUFACTURER_FED MANFCODE('F','E','D')
#define MANUFACTURER_FML MANFCODE('F','M','L')
#define MANUFACTURER_GBJ MANFCODE('G','B','J')
#define MANUFACTURER_GEC MANFCODE('G','E','C')
#define MANUFACTURER_GSP MANFCODE('G','S','P')
#define MANUFACTURER_GWF MANFCODE('G','W','F')
#define MANUFACTURER_HEG MANFCODE('H','E','G')
#define MANUFACTURER_HEL MANFCODE('H','E','L')
#define MANUFACTURER_HTC MANFCODE('H','T','C')
#define MANUFACTURER_HYD MANFCODE('H','Y','D')
#define MANUFACTURER_ICM MANFCODE('I','C','M')
#define MANUFACTURER_IDE MANFCODE('I','D','E')
#define MANUFACTURER_INV MANFCODE('I','N','V')
#define MANUFACTURER_ISK MANFCODE('I','S','K')
#define MANUFACTURER_IST MANFCODE('I','S','T')
#define MANUFACTURER_ITR MANFCODE('I','T','R')
#define MANUFACTURER_IWK MANFCODE('I','W','K')
#define MANUFACTURER_KAM MANFCODE('K','A','M')
#define MANUFACTURER_KHL MANFCODE('K','H','L')
#define MANUFACTURER_KKE MANFCODE('K','K','E')
#define MANUFACTURER_KNX MANFCODE('K','N','X')
#define MANUFACTURER_KRO MANFCODE('K','R','O')
#define MANUFACTURER_KST MANFCODE('K','S','T')
#define MANUFACTURER_LEM MANFCODE('L','E','M')
#define MANUFACTURER_LGB MANFCODE('L','G','B')
#define MANUFACTURER_LGD MANFCODE('L','G','D')
#define MANUFACTURER_LGZ MANFCODE('L','G','Z')
#define MANUFACTURER_LHA MANFCODE('L','H','A')
#define MANUFACTURER_LML MANFCODE('L','M','L')
#define MANUFACTURER_LSE MANFCODE('L','S','E')
#define MANUFACTURER_LSP MANFCODE('L','S','P')
#define MANUFACTURER_LUG MANFCODE('L','U','G')
#define MANUFACTURER_LSZ MANFCODE('L','S','Z')
#define MANUFACTURER_MAD MANFCODE('M','A','D')
#define MANUFACTURER_MEI MANFCODE('M','E','I')
#define MANUFACTURER_MKS MANFCODE('M','K','S')
#define MANUFACTURER_MNS MANFCODE('M','N','S')
#define MANUFACTURER_MPS MANFCODE('M','P','S')
#define MANUFACTURER_MTC MANFCODE('M','T','C')
#define MANUFACTURER_NIS MANFCODE('N','I','S')
#define MANUFACTURER_NMS MANFCODE('N','M','S')
#define MANUFACTURER_NRM MANFCODE('N','R','M')
#define MANUFACTURER_ONR MANFCODE('O','N','R')
#define MANUFACTURER_PAD MANFCODE('P','A','D')
#define MANUFACTURER_PMG MANFCODE('P','M','G')
#define MANUFACTURER_PRI MANFCODE('P','R','I')
#define MANUFACTURER_RAS MANFCODE('R','A','S')
#define MANUFACTURER_REL MANFCODE('R','E','L')
#define MANUFACTURER_RKE MANFCODE('R','K','E')
#define MANUFACTURER_SAP MANFCODE('S','A','P')
#define MANUFACTURER_SCH MANFCODE('S','C','H')
#define MANUFACTURER_SEN MANFCODE('S','E','N')
#define MANUFACTURER_SMC MANFCODE('S','M','C')
#define MANUFACTURER_SME MANFCODE('S','M','E')
#define MANUFACTURER_SML MANFCODE('S','M','L')
#define MANUFACTURER_SIE MANFCODE('S','I','E')
#define MANUFACTURER_SLB MANFCODE('S','L','B')
#define MANUFACTURER_SON MANFCODE('S','O','N')
#define MANUFACTURER_SOF MANFCODE('S','O','F')
#define MANUFACTURER_SPL MANFCODE('S','P','L')
#define MANUFACTURER_SPX MANFCODE('S','P','X')
#define MANUFACTURER_SVM MANFCODE('S','V','M')
#define MANUFACTURER_TCH MANFCODE('T','C','H')
#define MANUFACTURER_TIP MANFCODE('T','I','P')
#define MANUFACTURER_UAG MANFCODE('U','A','G')
#define MANUFACTURER_UGI MANFCODE('U','G','I')
#define MANUFACTURER_VES MANFCODE('V','E','S')
#define MANUFACTURER_VPI MANFCODE('V','P','I')
#define MANUFACTURER_WMO MANFCODE('W','M','O')
#define MANUFACTURER_YTE MANFCODE('Y','T','E')
#define MANUFACTURER_ZAG MANFCODE('Z','A','G')
#define MANUFACTURER_ZAP MANFCODE('Z','A','P')
#define MANUFACTURER_ZIV MANFCODE('Z','I','V')
#define MANUFACTURER_QDS MANFCODE('Q','D','S')

#endif
