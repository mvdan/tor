package main

import (
	"fmt"
	//"os"
	"os/exec"
	"io"
	"log"
	"archive/tar"
	"path/filepath"
)

func analyze(tr *tar.Reader) {
	for {
		hdr, err := tr.Next()
		if err == io.EOF {
			// end of tar archive
			break
		}
		if err != nil {
			log.Fatalln(err)
		}
		fmt.Printf("%s\n", hdr.Name)
	}

}

func main() {
	tarpaths, err := filepath.Glob("*.tar.*")
	if err != nil {
		log.Fatal(err)
	}

	for _,tp := range tarpaths {
		fmt.Printf("Parsing %s\n", tp)
		ext := filepath.Ext(tp)
		var catcmd string
		switch ext {
		case ".xz":
			catcmd = "xzcat"
		case ".bz2":
			catcmd = "bzcat"
		default:
			log.Printf("Unsupported format .tar%s in %s", ext, tp)
			continue
		}
		cmd := exec.Command(catcmd, tp)
		fr, err := cmd.StdoutPipe()
		if err != nil {
			log.Fatal(err)
		}
		//defer fr.Close()
		if err := cmd.Start(); err != nil {
			log.Fatal(err)
		}
		tr := tar.NewReader(fr)
		analyze(tr)
	}

}
