package main

import (
	"archive/tar"
	"bufio"
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"sort"
	"strings"
	"time"
)

type ByteSize float64

const (
	_           = iota
	KB ByteSize = 1 << (10 * iota)
	MB
	GB
)

func (b ByteSize) String() string {
	switch {
	case b >= GB:
		return fmt.Sprintf("%.2fGB", b/GB)
	case b >= MB:
		return fmt.Sprintf("%.2fMB", b/MB)
	case b >= KB:
		return fmt.Sprintf("%.2fKB", b/KB)
	}
	return fmt.Sprintf("%.2fB", b)
}

type Consensus struct {
	Time   time.Time
	Ids    []string
	Hashes []string
	Size   int64
}

type Consensuses []Consensus

type ConsensusStats struct {
	Data            Consensuses
	MeanEntrySize   ByteSize
	MeanMicroSize   ByteSize
	TotalSize       int64
	TotalEntryCount int
}

func (c Consensuses) Len() int {
	return len(c)
}

func (c Consensuses) Less(i, j int) bool {
	return c[i].Time.Before(c[j].Time)
}

func (c Consensuses) Swap(i, j int) {
	c[i], c[j] = c[j], c[i]
}

const (
	dateInForm  = "2006-01-02-15-04-05"
	dateOutForm = "2006-01-02-15-04-05"
)

func analyze(cs Consensuses, tr *tar.Reader) (Consensuses, ByteSize) {
	var totalMicroSize, totalMicroCount int64 = 0, 0
	for {
		hdr, err := tr.Next()
		if err == io.EOF {
			break
		}
		if err != nil {
			log.Fatalln(err)
		}
		if hdr.Typeflag != tar.TypeReg {
			//log.Println(hdr.Name)
			continue
		}

		if strings.Contains(hdr.Name, "/micro/") {
			totalMicroSize += hdr.Size
			totalMicroCount++
			continue
		}

		strTime := path.Base(hdr.Name)[:len("YYYY-MM-DD-HH-MM-SS")]
		parsedTime, err := time.Parse(dateInForm, strTime)
		c := Consensus{Time: parsedTime, Size: hdr.Size}
		scanner := bufio.NewScanner(tr)
		for scanner.Scan() {
			text := scanner.Text()
			switch text[0:2] {
			case "r ":
				fields := strings.Split(text, " ")
				if len(fields) < 3 {
					log.Fatal("Missing identity: " + text)
				}
				c.Ids = append(c.Ids, fields[2])
			case "m ":
				fields := strings.Split(text, " ")
				if len(fields) < 2 {
					log.Fatal("Missing hash: " + text)
				}
				c.Hashes = append(c.Hashes, fields[1])
				if len(c.Ids) != len(c.Hashes) {
					log.Fatal("The number of identities and hashes differ!")
				}
			default:
				continue
			}
		}
		if err := scanner.Err(); err != nil {
			log.Fatal(err)
		}
		cs = append(cs, c)
	}
	meanMicroSize := ByteSize(totalMicroSize / totalMicroCount)
	return cs, meanMicroSize
}

func prepare(cst *ConsensusStats) {
	if len(cst.Data) == 0 {
		fmt.Println("No data to show.")
		return
	}
	sort.Sort(cst.Data)
	fmt.Printf("== Global data ==\n")

	for _, c := range cst.Data {
		cst.TotalSize += c.Size
		cst.TotalEntryCount += len(c.Ids)
	}

	cst.MeanEntrySize = ByteSize(cst.TotalSize) / ByteSize(cst.TotalEntryCount)
	fmt.Printf("Note that ALL of the sizes shown are in uncompressed bytes\n")
	fmt.Printf("Number of consensuses: %d\n", len(cst.Data))
	fmt.Printf("Mean consensus entry size: %s\n", cst.MeanEntrySize.String())
	fmt.Printf("Mean microdescriptor size: %s\n", cst.MeanMicroSize.String())
}

func results(cst ConsensusStats, maxInterval int, keep int) {
	fmt.Printf("When keeping non-running relays for %d hours...\n", keep)
	var totalConsensusEntries int64 = 0
	cs := make([]map[string]bool, len(cst.Data))
	hs := make([]map[string]bool, len(cst.Data))
	for i := range cst.Data {
		c := make(map[string]bool)
		h := make(map[string]bool)
		for k := 0; k <= keep; k++ {
			if k > i { continue }
			for _, id := range cst.Data[i-k].Ids {
				c[id] = true
			}
			for _, hash := range cst.Data[i-k].Hashes {
				h[hash] = true
			}
		}
		totalConsensusEntries += int64(len(c))
		cs[i], hs[i] = c, h
	}

	meanConsensusSize := ByteSize(totalConsensusEntries) * (cst.MeanEntrySize / ByteSize(len(cs)))
	fmt.Printf("Mean consensus size: %s\n", meanConsensusSize.String())

	for interval := 1; interval <= maxInterval; interval++ {
		var totalDiffSize, totalMicroSize ByteSize = 0, 0
		for i := range cs {
			if interval > i { continue }
			oldIds := cs[i-interval]
			newIds := cs[i]
			oldHashes := hs[i-interval]
			newHashes := hs[i]
			for hash := range newHashes {
				_, e := oldHashes[hash]
				if !e {
					totalMicroSize += cst.MeanMicroSize
				}
			}
			for id := range newIds {
				_, e := oldIds[id]
				// Added
				if !e {
					totalDiffSize += cst.MeanEntrySize + ByteSize(3.0)
				}
			}
			for id := range oldIds {
				_, e := newIds[id]
				// Deleted
				if !e {
					totalDiffSize += ByteSize(6.0)
				}
			}
		}
		meanDiffSize := totalDiffSize / ByteSize(len(cs))
		fmt.Printf("Mean consensus diff size when interval is %dh: %s\n",
			interval, meanDiffSize.String())
		meanMicroSize := totalMicroSize / ByteSize(len(cs))
		fmt.Printf("Mean microdescriptor download size when interval is %dh: %s\n",
			interval, meanMicroSize.String())
	}
}

func main() {

	cst := ConsensusStats{}
	flag.Parse()

	for _, tp := range flag.Args() {
		fmt.Printf("Parsing %s\n", tp)
		ext := filepath.Ext(tp)
		var catcmd string
		switch ext {
		case ".xz":
			catcmd = "xzcat"
		case ".bz2":
			catcmd = "bzcat"
		case ".gz":
			catcmd = "zcat"
		default:
			log.Printf("Unsupported format .tar%s in %s", ext, tp)
			continue
		}
		cmd := exec.Command(catcmd, tp)
		fr, err := cmd.StdoutPipe()
		cmd.Stderr = os.Stderr
		if err != nil {
			log.Fatal(err)
		}
		defer fr.Close()
		if err := cmd.Start(); err != nil {
			log.Fatal(err)
		}
		tr := tar.NewReader(fr)
		cst.Data, cst.MeanMicroSize = analyze(cst.Data, tr)
	}
	prepare(&cst)
	for keep := 0; keep <= 12; keep++ {
		results(cst, 12, keep)
	}
}
