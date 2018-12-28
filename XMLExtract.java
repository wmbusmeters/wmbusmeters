//
// A utility to extract the contents of an
// XML containing an XML-Encrypted CipherValue.
//
// Compile with javac XMLExtract.java (requires jdk8 or above).
//
import javax.crypto.Cipher;
import javax.crypto.SecretKey;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.Base64;
import java.util.stream.Collectors;

public class XMLExtract
{
    static Cipher getCipherDecrypt(String password) throws Exception {
        byte[] key = Arrays.copyOf(password.toUpperCase().getBytes("UTF-8"), 16);
        Cipher c = Cipher.getInstance("AES/CBC/NoPadding");
        SecretKeySpec sks = new SecretKeySpec(key, "AES");
        IvParameterSpec ips = new IvParameterSpec(key);
        c.init(Cipher.DECRYPT_MODE, sks, ips);
        return c;
    }

    public static void main(String args[]) throws Exception
    {
        if (args.length < 2) {
            System.out.println("java -cp . XMLExtract [password] [encrypted_xml_file] ");
            return;
        }
        Cipher c = getCipherDecrypt(args[0]);
        String xml = Files.lines(Paths.get(args[1])).collect(Collectors.joining(""));
        String hex = xml.substring(xml.indexOf("<CipherValue>")+13, xml.indexOf("</CipherValue>"));
        byte[] raw = Base64.getDecoder().decode(hex);
        String plain = new String(c.doFinal(raw));
        System.out.println(plain);
    }
}
