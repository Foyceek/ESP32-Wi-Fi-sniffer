<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE eagle SYSTEM "eagle.dtd">
<eagle version="9.6.2">
<drawing>
<settings>
<setting alwaysvectorfont="no"/>
<setting verticaltext="up"/>
</settings>
<grid distance="0.1" unitdist="inch" unit="inch" style="lines" multiple="1" display="no" altdistance="0.01" altunitdist="inch" altunit="inch"/>
<layers>
<layer number="1" name="Top" color="4" fill="1" visible="no" active="no"/>
<layer number="16" name="Bottom" color="1" fill="1" visible="no" active="no"/>
<layer number="17" name="Pads" color="2" fill="1" visible="no" active="no"/>
<layer number="18" name="Vias" color="2" fill="1" visible="no" active="no"/>
<layer number="19" name="Unrouted" color="6" fill="1" visible="no" active="no"/>
<layer number="20" name="Dimension" color="24" fill="1" visible="no" active="no"/>
<layer number="21" name="tPlace" color="7" fill="1" visible="no" active="no"/>
<layer number="22" name="bPlace" color="7" fill="1" visible="no" active="no"/>
<layer number="23" name="tOrigins" color="15" fill="1" visible="no" active="no"/>
<layer number="24" name="bOrigins" color="15" fill="1" visible="no" active="no"/>
<layer number="25" name="tNames" color="7" fill="1" visible="no" active="no"/>
<layer number="26" name="bNames" color="7" fill="1" visible="no" active="no"/>
<layer number="27" name="tValues" color="7" fill="1" visible="no" active="no"/>
<layer number="28" name="bValues" color="7" fill="1" visible="no" active="no"/>
<layer number="29" name="tStop" color="7" fill="3" visible="no" active="no"/>
<layer number="30" name="bStop" color="7" fill="6" visible="no" active="no"/>
<layer number="31" name="tCream" color="7" fill="4" visible="no" active="no"/>
<layer number="32" name="bCream" color="7" fill="5" visible="no" active="no"/>
<layer number="33" name="tFinish" color="6" fill="3" visible="no" active="no"/>
<layer number="34" name="bFinish" color="6" fill="6" visible="no" active="no"/>
<layer number="35" name="tGlue" color="7" fill="4" visible="no" active="no"/>
<layer number="36" name="bGlue" color="7" fill="5" visible="no" active="no"/>
<layer number="37" name="tTest" color="7" fill="1" visible="no" active="no"/>
<layer number="38" name="bTest" color="7" fill="1" visible="no" active="no"/>
<layer number="39" name="tKeepout" color="4" fill="11" visible="no" active="no"/>
<layer number="40" name="bKeepout" color="1" fill="11" visible="no" active="no"/>
<layer number="41" name="tRestrict" color="4" fill="10" visible="no" active="no"/>
<layer number="42" name="bRestrict" color="1" fill="10" visible="no" active="no"/>
<layer number="43" name="vRestrict" color="2" fill="10" visible="no" active="no"/>
<layer number="44" name="Drills" color="7" fill="1" visible="no" active="no"/>
<layer number="45" name="Holes" color="7" fill="1" visible="no" active="no"/>
<layer number="46" name="Milling" color="3" fill="1" visible="no" active="no"/>
<layer number="47" name="Measures" color="7" fill="1" visible="no" active="no"/>
<layer number="48" name="Document" color="7" fill="1" visible="no" active="no"/>
<layer number="49" name="Reference" color="7" fill="1" visible="no" active="no"/>
<layer number="51" name="tDocu" color="7" fill="1" visible="no" active="no"/>
<layer number="52" name="bDocu" color="7" fill="1" visible="no" active="no"/>
<layer number="88" name="SimResults" color="9" fill="1" visible="yes" active="yes"/>
<layer number="89" name="SimProbes" color="9" fill="1" visible="yes" active="yes"/>
<layer number="90" name="Modules" color="5" fill="1" visible="yes" active="yes"/>
<layer number="91" name="Nets" color="2" fill="1" visible="yes" active="yes"/>
<layer number="92" name="Busses" color="1" fill="1" visible="yes" active="yes"/>
<layer number="93" name="Pins" color="2" fill="1" visible="no" active="yes"/>
<layer number="94" name="Symbols" color="4" fill="1" visible="yes" active="yes"/>
<layer number="95" name="Names" color="7" fill="1" visible="yes" active="yes"/>
<layer number="96" name="Values" color="7" fill="1" visible="yes" active="yes"/>
<layer number="97" name="Info" color="7" fill="1" visible="yes" active="yes"/>
<layer number="98" name="Guide" color="6" fill="1" visible="yes" active="yes"/>
</layers>
<schematic xreflabel="%F%N/%S.%C%R" xrefpart="/%S.%C%R">
<libraries>
<library name="ESP32_SNIFFER">
<packages>
<package name="TACT_BUTTON">
<wire x1="-1" y1="7.5" x2="-1" y2="-1.5" width="0.127" layer="22"/>
<wire x1="-1" y1="-1.5" x2="5.5" y2="-1.5" width="0.127" layer="22"/>
<wire x1="5.5" y1="-1.5" x2="5.5" y2="7.5" width="0.127" layer="22"/>
<wire x1="5.5" y1="7.5" x2="-1" y2="7.5" width="0.127" layer="22"/>
<pad name="3" x="0" y="0" drill="0.6" diameter="2.54" shape="octagon"/>
<pad name="4" x="4.5" y="0" drill="0.6" diameter="2.54" shape="octagon"/>
<pad name="1" x="0" y="6" drill="0.6" diameter="2.54" shape="octagon"/>
<pad name="2" x="4.5" y="6" drill="0.6" diameter="2.54" shape="octagon"/>
</package>
<package name="OLED_0.96&quot;">
<wire x1="-12.5" y1="12.5" x2="-12.5" y2="-12.5" width="0.127" layer="21"/>
<wire x1="-12.5" y1="-12.5" x2="12.5" y2="-12.5" width="0.127" layer="21"/>
<wire x1="12.5" y1="-12.5" x2="12.5" y2="12.5" width="0.127" layer="21"/>
<wire x1="12.5" y1="12.5" x2="-12.5" y2="12.5" width="0.127" layer="21"/>
<pad name="VCC" x="-1.27" y="10.16" drill="0.6" diameter="1.6764" shape="long" rot="R90"/>
<pad name="GND" x="-3.81" y="10.16" drill="0.6" diameter="1.6764" shape="long" rot="R90"/>
<pad name="SCL" x="1.27" y="10.16" drill="0.6" diameter="1.6764" shape="long" rot="R90"/>
<pad name="SDA" x="3.81" y="10.16" drill="0.6" diameter="1.6764" shape="long" rot="R90"/>
</package>
<package name="ESP32_1X8">
<wire x1="-20.32" y1="2.54" x2="-20.32" y2="-2.54" width="0.127" layer="22"/>
<wire x1="-20.32" y1="-2.54" x2="2.54" y2="-2.54" width="0.127" layer="22"/>
<wire x1="2.54" y1="-2.54" x2="2.54" y2="2.54" width="0.127" layer="22"/>
<wire x1="2.54" y1="2.54" x2="-20.32" y2="2.54" width="0.127" layer="22"/>
<pad name="3V3" x="0" y="0" drill="0.6" diameter="1.27" shape="long" rot="R90"/>
<pad name="SDA" x="-2.54" y="0" drill="0.6" diameter="1.27" shape="long" rot="R90"/>
<pad name="SCL" x="-5.08" y="0" drill="0.6" diameter="1.27" shape="long" rot="R90"/>
<pad name="HREF" x="-7.62" y="0" drill="0.6" diameter="1.27" shape="long" rot="R90"/>
<pad name="GND" x="-10.16" y="0" drill="0.6" diameter="1.27" shape="long" rot="R90"/>
<pad name="IO19" x="-12.7" y="0" drill="0.6" diameter="1.27" shape="long" rot="R90"/>
<pad name="CS2" x="-15.24" y="0" drill="0.6" diameter="1.27" shape="long" rot="R90"/>
<pad name="IO2" x="-17.78" y="0" drill="0.6" diameter="1.27" shape="long" rot="R90"/>
</package>
</packages>
<symbols>
<symbol name="TACT_BUTTON">
<circle x="0" y="0" radius="2.54" width="0.254" layer="94"/>
<wire x1="-5.08" y1="5.08" x2="-5.08" y2="-5.08" width="0.254" layer="94"/>
<wire x1="-5.08" y1="-5.08" x2="5.08" y2="-5.08" width="0.254" layer="94"/>
<wire x1="5.08" y1="-5.08" x2="5.08" y2="5.08" width="0.254" layer="94"/>
<wire x1="5.08" y1="5.08" x2="-5.08" y2="5.08" width="0.254" layer="94"/>
<pin name="1" x="-2.54" y="10.16" length="middle" rot="R270"/>
<pin name="2" x="2.54" y="10.16" length="middle" rot="R270"/>
<pin name="4" x="2.54" y="-10.16" length="middle" rot="R90"/>
<pin name="3" x="-2.54" y="-10.16" length="middle" rot="R90"/>
</symbol>
<symbol name="OLED_0.96&quot;">
<pin name="GND" x="-7.62" y="17.78" length="middle" rot="R270"/>
<pin name="VCC" x="-2.54" y="17.78" length="middle" rot="R270"/>
<pin name="SCL" x="2.54" y="17.78" length="middle" rot="R270"/>
<pin name="SDA" x="7.62" y="17.78" length="middle" rot="R270"/>
<wire x1="-15.24" y1="12.7" x2="15.24" y2="12.7" width="0.254" layer="94"/>
<wire x1="15.24" y1="12.7" x2="15.24" y2="-12.7" width="0.254" layer="94"/>
<wire x1="15.24" y1="-12.7" x2="-15.24" y2="-12.7" width="0.254" layer="94"/>
<wire x1="-15.24" y1="-12.7" x2="-15.24" y2="12.7" width="0.254" layer="94"/>
</symbol>
<symbol name="ESP32_1X8">
<pin name="3V3" x="17.78" y="-7.62" length="middle" rot="R90"/>
<pin name="SDA" x="12.7" y="-7.62" length="middle" rot="R90"/>
<pin name="SCL" x="7.62" y="-7.62" length="middle" rot="R90"/>
<pin name="HREF" x="2.54" y="-7.62" length="middle" rot="R90"/>
<pin name="GND" x="-2.54" y="-7.62" length="middle" rot="R90"/>
<pin name="IO19" x="-7.62" y="-7.62" length="middle" rot="R90"/>
<pin name="CS2" x="-12.7" y="-7.62" length="middle" rot="R90"/>
<pin name="IO2" x="-17.78" y="-7.62" length="middle" rot="R90"/>
<wire x1="22.86" y1="-2.54" x2="-22.86" y2="-2.54" width="0.254" layer="94"/>
<wire x1="-22.86" y1="-2.54" x2="-22.86" y2="5.08" width="0.254" layer="94"/>
<wire x1="-22.86" y1="5.08" x2="22.86" y2="5.08" width="0.254" layer="94"/>
<wire x1="22.86" y1="5.08" x2="22.86" y2="-2.54" width="0.254" layer="94"/>
</symbol>
</symbols>
<devicesets>
<deviceset name="TACT_BUTTON">
<gates>
<gate name="G$1" symbol="TACT_BUTTON" x="0" y="0"/>
</gates>
<devices>
<device name="" package="TACT_BUTTON">
<connects>
<connect gate="G$1" pin="1" pad="1"/>
<connect gate="G$1" pin="2" pad="2"/>
<connect gate="G$1" pin="3" pad="3"/>
<connect gate="G$1" pin="4" pad="4"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
</devices>
</deviceset>
<deviceset name="OLED_0.96&quot;">
<gates>
<gate name="G$1" symbol="OLED_0.96&quot;" x="0" y="0"/>
</gates>
<devices>
<device name="" package="OLED_0.96&quot;">
<connects>
<connect gate="G$1" pin="GND" pad="GND"/>
<connect gate="G$1" pin="SCL" pad="SCL"/>
<connect gate="G$1" pin="SDA" pad="SDA"/>
<connect gate="G$1" pin="VCC" pad="VCC"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
</devices>
</deviceset>
<deviceset name="ESP32_1X8">
<gates>
<gate name="G$1" symbol="ESP32_1X8" x="0" y="0"/>
</gates>
<devices>
<device name="" package="ESP32_1X8">
<connects>
<connect gate="G$1" pin="3V3" pad="3V3"/>
<connect gate="G$1" pin="CS2" pad="CS2"/>
<connect gate="G$1" pin="GND" pad="GND"/>
<connect gate="G$1" pin="HREF" pad="HREF"/>
<connect gate="G$1" pin="IO19" pad="IO19"/>
<connect gate="G$1" pin="IO2" pad="IO2"/>
<connect gate="G$1" pin="SCL" pad="SCL"/>
<connect gate="G$1" pin="SDA" pad="SDA"/>
</connects>
<technologies>
<technology name=""/>
</technologies>
</device>
</devices>
</deviceset>
</devicesets>
</library>
</libraries>
<attributes>
</attributes>
<variantdefs>
</variantdefs>
<classes>
<class number="0" name="default" width="0" drill="0">
</class>
</classes>
<parts>
<part name="U$1" library="ESP32_SNIFFER" deviceset="TACT_BUTTON" device=""/>
<part name="U$2" library="ESP32_SNIFFER" deviceset="TACT_BUTTON" device=""/>
<part name="U$3" library="ESP32_SNIFFER" deviceset="OLED_0.96&quot;" device=""/>
<part name="U$4" library="ESP32_SNIFFER" deviceset="ESP32_1X8" device=""/>
</parts>
<sheets>
<sheet>
<plain>
</plain>
<instances>
<instance part="U$1" gate="G$1" x="-55.88" y="0" smashed="yes"/>
<instance part="U$2" gate="G$1" x="-33.02" y="0" smashed="yes"/>
<instance part="U$3" gate="G$1" x="0" y="0" smashed="yes"/>
<instance part="U$4" gate="G$1" x="-5.08" y="35.56" smashed="yes"/>
</instances>
<busses>
</busses>
<nets>
<net name="SDA" class="0">
<segment>
<pinref part="U$3" gate="G$1" pin="SDA"/>
<pinref part="U$4" gate="G$1" pin="SDA"/>
<wire x1="7.62" y1="17.78" x2="7.62" y2="27.94" width="0.1524" layer="91"/>
</segment>
</net>
<net name="SCL" class="0">
<segment>
<pinref part="U$3" gate="G$1" pin="SCL"/>
<pinref part="U$4" gate="G$1" pin="SCL"/>
<wire x1="2.54" y1="17.78" x2="2.54" y2="27.94" width="0.1524" layer="91"/>
</segment>
</net>
<net name="VCC" class="0">
<segment>
<pinref part="U$3" gate="G$1" pin="VCC"/>
<wire x1="-2.54" y1="17.78" x2="-2.54" y2="22.86" width="0.1524" layer="91"/>
<wire x1="-2.54" y1="22.86" x2="12.7" y2="22.86" width="0.1524" layer="91"/>
<pinref part="U$4" gate="G$1" pin="3V3"/>
<wire x1="12.7" y1="22.86" x2="12.7" y2="27.94" width="0.1524" layer="91"/>
</segment>
</net>
<net name="IO19" class="0">
<segment>
<pinref part="U$4" gate="G$1" pin="IO19"/>
<wire x1="-12.7" y1="27.94" x2="-12.7" y2="17.78" width="0.1524" layer="91"/>
<wire x1="-12.7" y1="17.78" x2="-35.56" y2="17.78" width="0.1524" layer="91"/>
<pinref part="U$2" gate="G$1" pin="1"/>
<wire x1="-35.56" y1="17.78" x2="-35.56" y2="10.16" width="0.1524" layer="91"/>
</segment>
</net>
<net name="IO2" class="0">
<segment>
<pinref part="U$4" gate="G$1" pin="IO2"/>
<pinref part="U$1" gate="G$1" pin="1"/>
<wire x1="-22.86" y1="27.94" x2="-58.42" y2="27.94" width="0.1524" layer="91"/>
<wire x1="-58.42" y1="27.94" x2="-58.42" y2="10.16" width="0.1524" layer="91"/>
</segment>
</net>
<net name="GND" class="0">
<segment>
<pinref part="U$1" gate="G$1" pin="2"/>
<pinref part="U$2" gate="G$1" pin="2"/>
<wire x1="-53.34" y1="10.16" x2="-30.48" y2="10.16" width="0.1524" layer="91"/>
<pinref part="U$3" gate="G$1" pin="GND"/>
<pinref part="U$4" gate="G$1" pin="GND"/>
<wire x1="-7.62" y1="17.78" x2="-7.62" y2="21.59" width="0.1524" layer="91"/>
<wire x1="-7.62" y1="21.59" x2="-7.62" y2="27.94" width="0.1524" layer="91"/>
<wire x1="-30.48" y1="10.16" x2="-30.48" y2="21.59" width="0.1524" layer="91"/>
<wire x1="-30.48" y1="21.59" x2="-7.62" y2="21.59" width="0.1524" layer="91"/>
<junction x="-30.48" y="10.16"/>
<junction x="-7.62" y="21.59"/>
</segment>
</net>
</nets>
</sheet>
</sheets>
</schematic>
</drawing>
</eagle>
