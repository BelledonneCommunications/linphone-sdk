<?xml version='1.0'?>
<xsl:stylesheet
	version="1.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
       
	<xsl:template match="MEMORY_DUMP_REPORT">
		<html>
	 	 	<head>
	 	 		<title> CUnit Memory Debugger Dumper - All Allocation/Deallocation Report. </title>
	 	 	</head>
		 	 	
	 	 	<body bgcolor="e0e0f0">
	 	 		<xsl:apply-templates/>
	 	 	</body>
	 	 </html>
	</xsl:template>
	 	
	<xsl:template match="MD_HEADER">
		<div align="center">
			<h3> <b> CUnit - A Unit testing framework for C. </b>
			<br/> <a href="http://cunit.sourceforge.net/"> http://cunit.sourceforge.net/ </a> </h3>
		</div>	
	</xsl:template>
	 	
	<xsl:template match="MD_RUN_LISTING">
		<table cols="5" width="95%" align="center">
			<th width="35%">Allocation File</th>
			<th width="10%">Allocation Line</th>
			<th width="35%">Deallocation File</th>
			<th width="10%">Deallocation Line</th>
			<th width="10%">Pointer</th>
			<th width="10%">Data Size</th>
			<xsl:apply-templates/>
		</table>	
	</xsl:template>
	
	<xsl:for-each select="MD_RUN_RECORD">
		<tr> 
			<td> <xsl:value-of select="MD_SOURCE_FILE"/> </td>
			<td> <xsl:value-of select="MD_SOURCE_LINE"/> </td>
			<td> <xsl:value-of select="MD_DESTINATION_FILE"/> </td>
			<td> <xsl:value-of select="MD_DESTINATION_LINE"/> </td>
			<td> <xsl:value-of select="MD_POINTER"/> </td>
			<td> <xsl:value-of select="MD_SIZE"/> </td>
		</tr>
	</xsl:template>
	
	<xsl:template match="MD_SUMMARY">
		<p/>
		<table width="90%" rows="2" align="center">
			<tr align="center" bgcolor="skyblue"> <th colspan="5"> Cumulative Summary for Memory Debugger Dumper Run </th> </tr>
			<tr>
				<th width="50%" bgcolor="ffffc0" align="center"> Valid Records </th> <td> <xsl:value-of select="MD_SUMMARY_VALID_RECORDS" /> </td>
			</tr>
				
			<tr>
				<th width="50%" bgcolor="ffffc0" align="center"> Invalid Records </th> <td> <xsl:value-of select="MD_SUMMARY_INVALID_RECORDS" /> </td>
			</tr>

			<tr> 
				<th width="50%" bgcolor="ffffc0" align="center"> Total Number of Allocation/Deallocation Records </th> <td> <xsl:value-of select="MD_SUMMARY_TOTAL_RECORDS" /> </td>
			<tr>

		</table>
	</xsl:template>
	
	<xsl:template match="MD_FOOTER">
		<p/>
		<hr align="center" width="90%" color="red" />
		<h5 align="center"> 
	 		<xsl:apply-templates/>
	 	</h5>
	</xsl:template>
 	
</xsl:stylesheet>
