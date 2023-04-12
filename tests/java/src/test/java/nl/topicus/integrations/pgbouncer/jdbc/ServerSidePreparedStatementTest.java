package nl.topicus.integrations.pgbouncer.jdbc;

import static org.junit.jupiter.api.Assertions.*;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.Statement;
import java.sql.Types;

import org.junit.jupiter.api.Test;

public class ServerSidePreparedStatementTest extends AbstractPgBouncerTest {
  @Test
  public void test1() throws Exception {
    Connection conn = getPgBouncerConnection();

    PreparedStatement pstmt = conn.prepareStatement("SELECT ?");
    org.postgresql.PGStatement pgstmt = (org.postgresql.PGStatement)pstmt;
    conn.beginRequest();

    for (int i=1; i<=5; i++)
		{
      // queries++;
			pstmt.setInt(1,i);
			boolean usingServerPrepare = pgstmt.isUseServerPrepare();
			ResultSet rs = pstmt.executeQuery();
      rs.next();
      assertEquals(i, rs.getInt(1));

			System.out.println("Client "+conn.hashCode()+" / Backend "+"?"+" - Execution: "+i+", Used server side: " + usingServerPrepare + ", Result: "+rs.getInt(1));
			rs.close();
    }

    conn.commit();
  }

  @Test
  public void testStatementCleanup() throws Exception {
    Connection conn = getPgBouncerConnection();

    Statement stmt = conn.createStatement();
    stmt.execute("SELECT inet_client_port(), pg_backend_pid()");
    stmt.getResultSet().next();
    String clientPort = stmt.getResultSet().getString(1);
    String connBackend = stmt.getResultSet().getString(2);
    stmt.close();    

    int queries = 0;
    conn.beginRequest();
    for (int k=1; k<=100; k++)
    { 
      for (int i=1; i<=50; i++)
      {
        for (int h=1; h<=3; h++)
        {
          queries++;
          
          PreparedStatement pstmt = conn.prepareStatement("SELECT " + i + " + ?, ?");
          org.postgresql.PGStatement pgstmt = (org.postgresql.PGStatement)pstmt;
          
          pstmt.setInt(1, i);
          pstmt.setNull(2, Types.INTEGER);
          boolean usingServerPrepare = pgstmt.isUseServerPrepare();
          ResultSet rs = pstmt.executeQuery();
          rs.next();
          System.out.println("Client "+conn.hashCode()+" / Backend "+connBackend+" - Execution: "+i+", Used server side: " + usingServerPrepare + ", Expected: " + (i*2) + ", Result: "+rs.getInt(1));
          assertEquals(i*2, rs.getInt(1));
          rs.close();
          
          pstmt.close();
        }
      }
    }
    conn.commit();
    
    conn.close();

    System.out.println("Executed " + queries + " queries");
  }
}
