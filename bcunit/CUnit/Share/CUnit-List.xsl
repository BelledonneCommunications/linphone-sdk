<?xml version='1.0'?>
<xsl:stylesheet	version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">       
       
 	<xsl:template match="CUNIT_TEST_LIST_REPORT">
	 	 <html>
	 	 	<head>
	 	 		<title> CUnit - Logical Test Case Organization in Test Registry. </title>
	 	 	</head>
	 	 	
	 	 	<body bgcolor="e0e0f0">
	 	 		<xsl:apply-templates/>
	 	 	</body>
	 	 </html>
 	</xsl:template>
 	
 	<xsl:template match="CUNIT_HEADER">
 		<div align="center">
 			<h3> <b> CUnit - A Unit testing framework for C. </b>
 			<br/> <a href="http://cunit.sourceforge.net/"> http://cunit.sourceforge.net/ </a> </h3>
	 	</div>	
 	</xsl:template>
 	 	
 	<xsl:template match="CUNIT_LIST_TOTAL_SUMMARY">
 		<p/>
 		<table align="center" width="50%">
 			<xsl:apply-templates/>
 		</table>
 	</xsl:template>
 	
 	<xsl:template match="CUNIT_LIST_TOTAL_SUMMARY_RECORD">
 		<tr>
 			<td bgcolor="f0f0e0" width="70%">
 				<xsl:value-of select="CUNIT_LIST_TOTAL_SUMMARY_RECORD_TEXT" />
 			</td>
 			<td bgcolor="f0e0e0" align="center">
 				<xsl:value-of select="CUNIT_LIST_TOTAL_SUMMARY_RECORD_VALUE" />
 			</td>
 		</tr>
 	</xsl:template>
 	
 	<xsl:template match="CUNIT_ALL_TEST_LISTING">
 		<p/>
 		<div align="center"> <h2> Listing of All Tests </h2> </div>
 		<table align="center" width="90%">
 			<xsl:apply-templates/>
 		</table>
 	</xsl:template>
 	
 	<xsl:template match="CUNIT_ALL_TEST_LISTING_GROUP">
 		<xsl:apply-templates/>
  	</xsl:template>
 	
 	<xsl:template match="CUNIT_ALL_TEST_LISTING_GROUP_DEFINITION">
 		<tr>
 			<td width="20%" bgcolor="f0e0f0"> Group Name </td>
 			<td colspan="5" width="70%" bgcolor="e0f0f0" > <xsl:value-of select="GROUP_NAME" /> </td>
 		</tr>
 		<tr>
 			<td bgcolor="f0e0f0" width="20%"> Initialize Value </td>
 			<td bgcolor="e0f0f0" width="13%"> <xsl:value-of select="INITIALIZE_VALUE" /> </td>
 			
 			<td bgcolor="f0e0f0" width="20%"> Cleanup Value </td>
 			<td bgcolor="e0f0f0" width="13%"> <xsl:value-of select="CLEANUP_VALUE" /> </td>
 			
 			<td bgcolor="f0e0f0" width="20%"> Number of Tests </td>
 			<td bgcolor="e0f0f0" width="13%"> <xsl:value-of select="TEST_COUNT_VALUE" /> </td>
 		</tr>
   	</xsl:template>
 	
 	<xsl:template match="CUNIT_ALL_TEST_LISTING_GROUP_TESTS">
 		<tr>
 			<td align="center" colspan="6" bgcolor="e0f0d0"> Test Cases </td>
 		</tr>
 		<xsl:for-each select="TEST_CASE_NAME">
 			<tr> <td align="left" colspan="6" bgcolor="e0e0d0"> <xsl:apply-templates/> </td> </tr>
 		</xsl:for-each>
 		<tr bgcolor="00ccff"> <td colspan="6"> </td> </tr>
  	</xsl:template>
 	 	
 	<xsl:template match="CUNIT_FOOTER">
 		<p/>
 		<hr align="center" width="90%" color="red" />
		<h5 align="center">
 			<xsl:apply-templates/>
 		</h5>
 	</xsl:template>
 	 		
</xsl:stylesheet>