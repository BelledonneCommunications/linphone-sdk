<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="xml" indent="yes" />
<xsl:template match="/">
   <testsuites>
      <xsl:for-each select="//BCUNIT_RUN_SUITE_SUCCESS">
          <xsl:variable name="suiteName" select="normalize-space(SUITE_NAME/text())"/>
	      <xsl:variable name="numberOfTests" select="count(BCUNIT_RUN_TEST_RECORD/BCUNIT_RUN_TEST_SUCCESS)"/>
	      <xsl:variable name="numberOfFailures" select="count(BCUNIT_RUN_TEST_RECORD/BCUNIT_RUN_TEST_FAILURE)"/>
      <testsuite
      	name="{$suiteName}"
      	tests="{$numberOfTests}"
		time="0"
		failures="{$numberOfFailures}"
		errors="0"
		skipped="0">
		
			<xsl:for-each select="BCUNIT_RUN_TEST_RECORD/BCUNIT_RUN_TEST_SUCCESS">
				<xsl:variable name="testname" select="normalize-space(TEST_NAME/text())"></xsl:variable>
				<testcase classname="{$suiteName}" name="{$testname}" time="0.0">
				</testcase>
			</xsl:for-each>
			
			<xsl:for-each select="BCUNIT_RUN_TEST_RECORD/BCUNIT_RUN_TEST_FAILURE">
				<xsl:variable name="testname" select="normalize-space(TEST_NAME/text())"></xsl:variable>
				<testcase classname="{$suiteName}" name="{$testname}" time="0.0">
					<failure>
Test failed at line <xsl:value-of select="LINE_NUMBER"></xsl:value-of> in file <xsl:value-of select="FILE_NAME"></xsl:value-of>: <xsl:value-of select="CONDITION"></xsl:value-of>
					</failure>
				</testcase>
			</xsl:for-each>
		
		
      </testsuite>

      </xsl:for-each>
   </testsuites>
</xsl:template>
</xsl:stylesheet>