import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;

public class Analyze {

    public static void main(String[] args)
    {
        String match = args.length > 1 ? args[1] : "";
        BufferedReader reader;
        try {
            reader = new BufferedReader(new FileReader(
                                            args[0]));
            String prevline = reader.readLine();
            String line = reader.readLine();
            while (line != null)
            {
                if (line.indexOf(match) >= 0)
                {
                    boolean space = false;
                    for (int i=0; i<line.length()-4; i+=2)
                    {
                        if (i < prevline.length()-1 && i < line.length()-1 &&
                            line.charAt(i) == prevline.charAt(i) &&
                            line.charAt(i+1) == prevline.charAt(i+1))
                        {
                            space = true;
                            System.out.print("  ");
                        }
                        else
                        {
                            /*if (space) {
                                System.out.print(" ("+i/2+")");
                                space = false;
                                }*/
                            System.out.print(line.charAt(i));
                            System.out.print(line.charAt(i+1));
                        }
                    }
                    System.out.println();
                    prevline = line;
                }
                line = reader.readLine();
            }
            reader.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

}
