package nl.topicus.integrations.pgbouncer.jdbc;

import java.lang.Runnable;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.Statement;
import java.sql.Types;
import java.util.List;
import java.util.ArrayList;
import java.util.concurrent.Executors;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Callable;
import java.util.Scanner;

public class SimplePgBouncerQuerier {
  private static Connection getPgBouncerConnection() throws SQLException {
    String uri = String.format("jdbc:postgresql://localhost:6432/postgres?prepareThreshold=2&preparedStatementCacheQueries=10");
    Connection conn = DriverManager.getConnection(uri, "pgbouncer", "fake");
    // conn.setAutoCommit(false);
    return conn;
  }

  public static void main(String[] args) {
    Scanner userInput = new Scanner(System.in);

    try {
        for (int z=11; z>=0; z--) {
            Connection conn = getPgBouncerConnection();
			// conn.setTransactionIsolation(0);
			// conn.beginRequest();
			
			// conn.rollback();

            for (int k=0; k<=7; k++) {
                for (int i=0; i<=9; i++) {
                    PreparedStatement pstmt = conn.prepareStatement("SELECT ?, " + k);
                    org.postgresql.PGStatement pgstmt = (org.postgresql.PGStatement)pstmt;

					String s = "";
					for (int x=0; x<=7*i; x++) {
						s += "x";
					}
					
                    pstmt.setString(1, s);
                    ResultSet rs = pstmt.executeQuery();
                    rs.next();

                    boolean usingServerPrepare = pgstmt.isUseServerPrepare();

                    // System.out.println("Client "+conn.hashCode()+", ServerPrepare="+usingServerPrepare+", result="+rs.getInt(1));
                    rs.close();

                    pstmt.close();
                }
            }

            if (z==-1) {
				System.out.println("test done, press enter to exit");
                userInput.nextLine();
            }
			
        	conn.close();
        }

        

    }
    catch (Exception e) {
      System.out.println(e);
    }
  }
}

 